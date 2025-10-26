#include "file_transfer.h"
#include "../utils/utils.h"
#include "../utils/file_utils.h"
#include <sys/sendfile.h>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <vector>
#include <cerrno>
#include <sys/stat.h>
#include <sys/socket.h>
#include <cstring>
#include <poll.h>

std::string format_size(long long bytes) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_index = 0;
    double size = bytes;

    while (size >= 1024.0 && unit_index < 4) {
        size /= 1024.0;
        unit_index++;
    }

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2) << size << " " << units[unit_index];
    return ss.str();
}

int calculate_percentage(long long current, long long total) {
    if (total == 0) return 0;
    return static_cast<int>((current * 100) / total);
}

bool stream_file(int fd, const std::string& filepath, const std::string& content_type,
                const std::string& filename, bool as_attachment,
                std::atomic<bool>* interrupted, int timeout_seconds) {
    struct stat st;
    if (::stat(filepath.c_str(), &st) != 0) {
        vlog("stream_file: stat failed for " + filepath);
        return false;
    }

    off_t file_size = st.st_size;
    off_t total_sent = 0;

    auto last_report_time = std::chrono::steady_clock::now();
    auto transfer_start_time = std::chrono::steady_clock::now();

    auto last_progress_time = transfer_start_time;
    long long last_sent_bytes = 0;

    int last_reported_percent = -1;


    vlog("Starting file send: " + filename + " (" + format_size(file_size) + ")");

    std::ostringstream header_stream;
    header_stream << "HTTP/1.1 200 OK\r\n"
                  << "Content-Type: " + content_type + "\r\n"
                  << "Content-Length: " << file_size << "\r\n";

    if (as_attachment && !filename.empty()) {
        header_stream << "Content-Disposition: attachment; filename=\"" << filename << "\"\r\n";
    }
    header_stream << "\r\n";

    std::string headers = header_stream.str();

    auto header_start_time = std::chrono::steady_clock::now();
    ssize_t header_sent = 0;
    while (header_sent < (ssize_t)headers.size()) {
        if (interrupted && *interrupted) {
            vlog("Header send interrupted by user");
            return false;
        }

        auto current_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - header_start_time);
        if (elapsed.count() > timeout_seconds) {
            vlog("Header send timeout");
            return false;
        }

        ssize_t sent = ::send(fd, headers.c_str() + header_sent, headers.size() - header_sent, MSG_NOSIGNAL);
        if (sent <= 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(10000);
                continue;
            }
            vlog("stream_file: Header send failed");
            return false;
        }
        header_sent += sent;
    }

    int file_fd = open(filepath.c_str(), O_RDONLY);
    if (file_fd >= 0) {
        off_t offset = 0;

        while (total_sent < file_size) {
            if (interrupted && *interrupted) {
                vlog("File send interrupted by user");
                close(file_fd);
                return false;
            }

            // --- REPLACED: use inactivity (no-progress) timeout instead of absolute elapsed ---
            auto current_time = std::chrono::steady_clock::now();
            if (total_sent > last_sent_bytes) {
                last_progress_time = current_time;
                last_sent_bytes = total_sent;
            }
            auto inactivity_elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_progress_time);
            if (inactivity_elapsed.count() > timeout_seconds) {
                vlog("File send timeout (no progress for " + std::to_string(inactivity_elapsed.count()) + "s)");
                close(file_fd);
                return false;
            }
            // --- end replacement ---

            auto op_start_time = std::chrono::steady_clock::now();
            bool op_completed = false;

            while (!op_completed) {
                if (interrupted && *interrupted) {
                    vlog("File send interrupted by user");
                    close(file_fd);
                    return false;
                }

                auto op_current_time = std::chrono::steady_clock::now();
                auto op_elapsed = std::chrono::duration_cast<std::chrono::seconds>(op_current_time - op_start_time);
                if (op_elapsed.count() > timeout_seconds) {
                    vlog("Send operation timeout");
                    close(file_fd);
                    return false;
                }

                ssize_t result = sendfile(fd, file_fd, &offset, file_size - total_sent);

                if (result > 0) {
                    total_sent += result;
                    // update progress tracking for inactivity timeout
                    last_progress_time = std::chrono::steady_clock::now();
                    last_sent_bytes = total_sent;
                    op_completed = true;
                } else if (result == 0) {
                    if (total_sent >= file_size) {
                        op_completed = true;
                    } else {
                        vlog("sendfile returned 0 but file not completely sent");
                        close(file_fd);
                        return false;
                    }
                } else {
                    if (errno == EINTR) continue;
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        usleep(10000);
                        continue;
                    }
                    vlog("stream_file: sendfile failed");
                    close(file_fd);
                    return false;
                }
            }

            auto report_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(report_time - last_report_time);
            int current_percent = calculate_percentage(total_sent, file_size);

            bool should_log = false;
            if (current_percent >= 100) {
                should_log = true;
            } else if (elapsed.count() >= 2) {
                should_log = true;
            } else if (current_percent != last_reported_percent && current_percent % 10 == 0) {
                should_log = true;
            }

            if (should_log && is_verbose()) {
                vlog("Send: " + std::to_string(current_percent) + "% - " +
                     format_size(total_sent) + " / " + format_size(file_size));
                last_report_time = report_time;
                last_reported_percent = current_percent;
            }
        }

        close(file_fd);
        vlog("File send completed: " + filename + " (" + format_size(total_sent) + ")");
        return true;
    }

    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        vlog("stream_file: Failed to open file");
        return false;
    }

    const size_t buffer_size = 1024 * 1024;
    std::vector<char> buffer(buffer_size);

    while (file.read(buffer.data(), buffer_size) || file.gcount() > 0) {
        if (interrupted && *interrupted) {
            vlog("File send interrupted by user");
            file.close();
            return false;
        }

        // --- REPLACED: use inactivity (no-progress) timeout instead of absolute elapsed ---
        auto current_time = std::chrono::steady_clock::now();
        if (total_sent > last_sent_bytes) {
            last_progress_time = current_time;
            last_sent_bytes = total_sent;
        }
        auto inactivity_elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_progress_time);
        if (inactivity_elapsed.count() > timeout_seconds) {
            vlog("File send timeout (no progress for " + std::to_string(inactivity_elapsed.count()) + "s)");
            file.close();
            return false;
        }
        // --- end replacement ---

        ssize_t bytes_read = file.gcount();
        ssize_t chunk_sent = 0;

        while (chunk_sent < bytes_read) {
            if (interrupted && *interrupted) {
                vlog("File send interrupted by user");
                file.close();
                return false;
            }

            auto op_start_time = std::chrono::steady_clock::now();
            bool op_completed = false;

            while (!op_completed) {
                if (interrupted && *interrupted) {
                    vlog("File send interrupted by user");
                    file.close();
                    return false;
                }

                auto op_current_time = std::chrono::steady_clock::now();
                auto op_elapsed = std::chrono::duration_cast<std::chrono::seconds>(op_current_time - op_start_time);
                if (op_elapsed.count() > timeout_seconds) {
                    vlog("Send operation timeout");
                    file.close();
                    return false;
                }

                ssize_t sent = ::send(fd, buffer.data() + chunk_sent, bytes_read - chunk_sent, MSG_NOSIGNAL);
                if (sent > 0) {
                    chunk_sent += sent;
                    total_sent += sent;
                    // update progress tracking for inactivity timeout
                    last_progress_time = std::chrono::steady_clock::now();
                    last_sent_bytes = total_sent;
                    op_completed = true;
                } else if (sent == 0) {
                    vlog("send returned 0 (connection closed)");
                    file.close();
                    return false;
                } else {
                    if (errno == EINTR) continue;
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        usleep(10000);
                        continue;
                    }
                    vlog("stream_file: send failed");
                    file.close();
                    return false;
                }
            }

            auto report_time = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(report_time - last_report_time);
            int current_percent = calculate_percentage(total_sent, file_size);

            bool should_log = false;
            if (current_percent >= 100) {
                should_log = true;
            } else if (elapsed.count() >= 2) {
                should_log = true;
            } else if (current_percent != last_reported_percent && current_percent % 10 == 0) {
                should_log = true;
            }

            if (should_log && is_verbose()) {
                vlog("Send: " + std::to_string(current_percent) + "% - " +
                     format_size(total_sent) + " / " + format_size(file_size));
                last_report_time = report_time;
                last_reported_percent = current_percent;
            }
        }

        if (file.eof()) {
            break;
        }
    }

    file.close();
    vlog("File send completed: " + filename + " (" + format_size(total_sent) + ")");
    return true;
}

bool stream_receive_file(int fd, long long content_length, const std::string& boundary,
                        const std::string& outname, long long max_size,
                        std::atomic<bool>* interrupted, int timeout_seconds) {
    std::string temp_path = outname + ".tmp." + random_token(8);
    vlog("Starting file receive: " + outname + " (" + format_size(content_length) + " expected)");

    std::ofstream file(temp_path, std::ios::binary);
    if (!file.is_open()) {
        vlog("Failed to open temp file: " + temp_path);
        return false;
    }

    const size_t buffer_size = 64 * 1024;
    std::vector<char> buffer(buffer_size);

    long long total_received = 0;

    auto last_report_time = std::chrono::steady_clock::now();
    auto transfer_start_time = std::chrono::steady_clock::now();

    auto last_progress_time = transfer_start_time;
    long long last_received_bytes = 0;

    int last_reported_percent = -1;



    enum ParseState { FIND_BOUNDARY, IN_HEADERS, IN_FILE_DATA, COMPLETE };
    ParseState state = FIND_BOUNDARY;

    std::string search_buffer;
    std::string boundary_delimiter = "--" + boundary;
    std::string end_boundary = boundary_delimiter + "--";

    vlog("Multipart boundary: " + boundary_delimiter);

    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;

    while (total_received < content_length && state != COMPLETE) {
        if (interrupted && *interrupted) {
            vlog("File receive interrupted by user");
            file.close();
            unlink(temp_path.c_str());
            return false;
        }


        auto current_time = std::chrono::steady_clock::now();
        if (total_received > last_received_bytes) {
            last_progress_time = current_time;
            last_received_bytes = total_received;
        }
        auto inactivity_elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_progress_time);
        if (inactivity_elapsed.count() > timeout_seconds) {
            vlog("File receive timeout (no progress for " + std::to_string(inactivity_elapsed.count()) + "s)");
            file.close();
            unlink(temp_path.c_str());
            return false;
        }



        int poll_result = poll(&pfd, 1, 100);

        if (poll_result < 0) {
            if (errno == EINTR) continue;
            vlog("Poll failed during receive");
            file.close();
            unlink(temp_path.c_str());
            return false;
        }

        if (poll_result == 0) {
            continue;
        }

        auto op_start_time = std::chrono::steady_clock::now();
        bool op_completed = false;

        while (!op_completed) {
            if (interrupted && *interrupted) {
                vlog("File receive interrupted by user");
                file.close();
                unlink(temp_path.c_str());
                return false;
            }

            auto op_current_time = std::chrono::steady_clock::now();
            auto op_elapsed = std::chrono::duration_cast<std::chrono::seconds>(op_current_time - op_start_time);
            if (op_elapsed.count() > timeout_seconds) {
                vlog("Receive operation timeout");
                file.close();
                unlink(temp_path.c_str());
                return false;
            }

            ssize_t to_read = std::min((long long)buffer_size, content_length - total_received);
            ssize_t bytes_read = ::recv(fd, buffer.data(), to_read, 0);

            if (bytes_read > 0) {
                total_received += bytes_read;
                op_completed = true;

                if (max_size > 0 && total_received > max_size) {
                    vlog("Size limit exceeded");
                    file.close();
                    unlink(temp_path.c_str());
                    return false;
                }

                search_buffer.append(buffer.data(), bytes_read);
            } else if (bytes_read == 0) {
                vlog("Connection closed by client");
                file.close();
                unlink(temp_path.c_str());
                return false;
            } else {
                if (errno == EINTR) continue;
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    usleep(10000);
                    continue;
                }
                vlog("Receive failed");
                file.close();
                unlink(temp_path.c_str());
                return false;
            }
        }

        while (search_buffer.size() > 0 && state != COMPLETE) {
            switch (state) {
                case FIND_BOUNDARY: {
                    size_t boundary_pos = search_buffer.find(boundary_delimiter);
                    if (boundary_pos != std::string::npos) {
                        vlog("Found boundary, moving to headers");
                        search_buffer = search_buffer.substr(boundary_pos + boundary_delimiter.size());
                        state = IN_HEADERS;
                    } else {
                        if (search_buffer.size() > boundary_delimiter.size()) {
                            search_buffer = search_buffer.substr(search_buffer.size() - boundary_delimiter.size());
                        }
                        goto next_chunk;
                    }
                    break;
                }

                case IN_HEADERS: {
                    size_t headers_end = search_buffer.find("\r\n\r\n");
                    if (headers_end != std::string::npos) {
                        vlog("Headers complete, starting file data");
                        search_buffer = search_buffer.substr(headers_end + 4);
                        state = IN_FILE_DATA;
                    } else {
                        goto next_chunk;
                    }
                    break;
                }

                case IN_FILE_DATA: {
                    size_t next_boundary_pos = search_buffer.find(boundary_delimiter);
                    size_t end_boundary_pos = search_buffer.find(end_boundary);

                    size_t boundary_pos = std::min(next_boundary_pos, end_boundary_pos);

                    if (boundary_pos != std::string::npos) {
                        file.write(search_buffer.data(), boundary_pos);
                        vlog("Found boundary, file data complete");
                        state = COMPLETE;
                        break;
                    }

                    size_t safe_write_size = search_buffer.size();
                    if (safe_write_size > boundary_delimiter.size()) {
                        safe_write_size -= boundary_delimiter.size();
                    }

                    if (safe_write_size > 0) {
                        file.write(search_buffer.data(), safe_write_size);
                        search_buffer = search_buffer.substr(safe_write_size);
                    }

                    goto next_chunk;
                    break;
                }

                case COMPLETE:
                    break;
            }
        }

        next_chunk:

        if (!file.good()) {
            vlog("File write failed");
            file.close();
            unlink(temp_path.c_str());
            return false;
        }

        auto report_time = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(report_time - last_report_time);
        int current_percent = calculate_percentage(total_received, content_length);

        bool should_log = false;
        if (current_percent >= 100) {
            should_log = true;
        } else if (elapsed.count() >= 2) {
            should_log = true;
        } else if (current_percent != last_reported_percent && current_percent % 10 == 0) {
            should_log = true;
        }

        if (should_log && is_verbose()) {
            vlog("Receive: " + std::to_string(current_percent) + "% - " +
                 format_size(total_received) + " / " + format_size(content_length));
            last_report_time = report_time;
            last_reported_percent = current_percent;
        }
    }

    file.close();

    if (state != COMPLETE) {
        vlog("Warning: File receive completed but multipart parsing didn't find end boundary");
    }

    vlog("File receive completed, renaming...");

    if (rename(temp_path.c_str(), outname.c_str()) != 0) {
        vlog("Rename failed");
        unlink(temp_path.c_str());
        return false;
    }

    vlog("File receive completed: " + outname + " (" + format_size(total_received) + ")");
    return true;
}

