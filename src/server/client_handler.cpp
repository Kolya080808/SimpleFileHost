#include "client_handler.h"
#include "http_handlers.h"
#include "file_transfer.h"
#include "../utils/utils.h"
#include "../utils/file_utils.h"
#include "../utils/server_utils.h"
#include <sstream>
#include <poll.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <cstring>

ClientHandler::ClientHandler(const ServerOptions& opts, int fd) 
    : opts_(opts), fd_(fd) {}

bool ClientHandler::read_headers(std::string& headers) {
    char chunk[4096];
    ssize_t r;
    bool headers_complete = false;

    struct pollfd client_fd;
    client_fd.fd = fd_;
    client_fd.events = POLLIN;
    
    auto start_time = std::chrono::steady_clock::now();
    
    while (true) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
        if (elapsed.count() > opts_.socket_timeout_seconds) {
            if (on_log) on_log("Header read timeout from " + get_client_ip(fd_));
            return false;
        }
        
        if (opts_.interrupted && *opts_.interrupted) {
            return false;
        }
        
        int poll_res = poll(&client_fd, 1, 100);
        if (poll_res < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (poll_res == 0) continue;
        
        r = recv(fd_, chunk, sizeof(chunk), 0);
        if (r > 0) {
            headers.append(chunk, r);
            if (headers.find("\r\n\r\n") != std::string::npos) {
                return true;
            }
        } else if (r == 0) {
            return false;
        } else {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                return false;
            }
        }
    }
}

void ClientHandler::send_response(const std::string& response) {
    ssize_t sent = 0;
    auto start_time = std::chrono::steady_clock::now();
    
    while (sent < (ssize_t)response.size()) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
        if (elapsed.count() > opts_.socket_timeout_seconds) {
            break;
        }
        
        if (opts_.interrupted && *opts_.interrupted) {
            break;
        }
        
        ssize_t result = send(fd_, response.c_str() + sent, response.size() - sent, MSG_NOSIGNAL);
        if (result > 0) {
            sent += result;
        } else if (result == 0) {
            break;
        } else {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(10000);
                continue;
            }
            break;
        }
    }
}

void ClientHandler::send_error(int code, const std::string& message) {
    std::ostringstream resp;
    resp << "HTTP/1.1 " << code << " " << (code == 404 ? "Not Found" : 
                                           code == 405 ? "Method Not Allowed" :
                                           code == 413 ? "Payload Too Large" : "Error")
         << "\r\nContent-Length: " << message.size() << "\r\n\r\n" << message;
    send_response(resp.str());
}

void ClientHandler::handle_get_request(const std::string& path, const std::string& headers) {
    if (path == "/" + opts_.token) {
        if (opts_.mode == "get") {
            if (on_log) on_log("Serving upload page to " + get_client_ip(fd_));
            auto html = html_upload_page(opts_.token);
            std::ostringstream resp;
            resp << "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: "
                 << html.size() << "\r\n\r\n" << html;
            send_response(resp.str());
        } else {
            if (on_log) on_log("Serving download page to " + get_client_ip(fd_));
            bool can_preview = false;
            struct stat file_stat;
            if (stat(opts_.path.c_str(), &file_stat) == 0) {
                can_preview = (mime_type(opts_.path).rfind("text/", 0) == 0) &&
                             (file_stat.st_size <= 1024 * 1024);
            }

            auto html = html_download_page(opts_.token, file_basename(opts_.path), can_preview);
            std::ostringstream resp;
            resp << "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: "
                 << html.size() << "\r\n\r\n" << html;
            send_response(resp.str());
        }
    } else if (path == "/" + opts_.token + "/file" && opts_.mode == "send") {
        if (on_log) on_log("Starting file download to " + get_client_ip(fd_));
        
        if (!file_exists(opts_.path)) {
            if (on_log) on_log("File not found: " + opts_.path);
            send_error(404, "404 File Not Found");
            return;
        }
        
        std::string filename = file_basename(opts_.path);
        bool success = stream_file(fd_, opts_.path, mime_type(opts_.path), filename, true, 
                                 opts_.interrupted);
        if (success) {
            if (on_log) on_log("File served to client: " + filename);
            if (on_client_done) on_client_done();
        } else {
            if (on_log) on_log("File download failed for " + get_client_ip(fd_));
            send_error(404, "404 File Not Found");
        }
    } else if (path == "/" + opts_.token + "/raw" && opts_.mode == "send") {
        if (on_log) on_log("Serving raw file to " + get_client_ip(fd_));
        
        struct stat file_stat;
        if (stat(opts_.path.c_str(), &file_stat) == 0 &&
            file_stat.st_size <= 1024 * 1024 &&
            mime_type(opts_.path).rfind("text/", 0) == 0) {

            bool success = stream_file(fd_, opts_.path, "text/plain; charset=utf-8", "", false, 
                                     opts_.interrupted);
            if (!success) {
                if (on_log) on_log("Raw file serve failed for " + get_client_ip(fd_));
                send_error(404, "404 File Not Found");
            }
        } else {
            if (on_log) on_log("Raw file preview not available for " + get_client_ip(fd_));
            send_error(404, "Preview not available for this file");
        }
    } else {
        if (on_log) on_log("404 Not Found: " + path + " from " + get_client_ip(fd_));
        send_error(404, "404 Not Found");
    }
}

void ClientHandler::handle_post_request(const std::string& path, const std::string& headers) {
    if (opts_.mode != "get") {
        send_error(405, "405 Method Not Allowed");
        return;
    }

    if (on_log) on_log("Starting file upload from " + get_client_ip(fd_));
    
    std::string boundary;
    auto content_type_pos = headers.find("Content-Type:");
    if (content_type_pos != std::string::npos) {
        auto boundary_pos = headers.find("boundary=", content_type_pos);
        if (boundary_pos != std::string::npos) {
            boundary_pos += 9; 
            size_t boundary_end = headers.find("\r\n", boundary_pos);
            if (boundary_end == std::string::npos) {
                boundary_end = headers.size();
            }
            boundary = headers.substr(boundary_pos, boundary_end - boundary_pos);
            if (boundary.size() >= 2 && boundary[0] == '"' && boundary[boundary.size()-1] == '"') {
                boundary = boundary.substr(1, boundary.size() - 2);
            }
            if (on_log) on_log("Extracted boundary: " + boundary);
        }
    }
    
    if (boundary.empty()) {
        if (on_log) on_log("Warning: No boundary found in headers from " + get_client_ip(fd_));
        size_t body_start = headers.find("\r\n\r\n");
        if (body_start != std::string::npos) {
            std::string body_part = headers.substr(body_start + 4, 200);
            size_t boundary_pos = body_part.find("--");
            if (boundary_pos != std::string::npos) {
                size_t boundary_end = body_part.find("\r\n", boundary_pos);
                if (boundary_end != std::string::npos) {
                    boundary = body_part.substr(boundary_pos + 2, boundary_end - boundary_pos - 2);
                    if (on_log) on_log("Extracted boundary from body: " + boundary);
                }
            }
        }
    }

    std::string outname;
    if (opts_.path.empty()) {
        outname = "upload_" + random_token(8);
    } else {
        outname = opts_.path;
    }

    long long content_len = extract_content_length(headers);
    bool success = stream_receive_file(fd_, content_len, boundary, outname, opts_.max_size, 
                                     opts_.interrupted);
    if (success) {
        if (on_log) on_log("File uploaded from " + get_client_ip(fd_) + ": " + outname);
        std::string success_msg = "<html><body><h2>Upload successful!</h2></body></html>";
        std::ostringstream resp;
        resp << "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: "
             << success_msg.size() << "\r\n\r\n" << success_msg;
        send_response(resp.str());
        
        if (on_client_done) on_client_done();
    } else {
        if (on_log) on_log("File upload failed from " + get_client_ip(fd_));
        std::string error_msg = "<html><body><h2>Upload failed!</h2></body></html>";
        std::ostringstream resp;
        resp << "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: "
             << error_msg.size() << "\r\n\r\n" << error_msg;
        send_response(resp.str());
    }
}

void ClientHandler::handle() {
    std::string headers;
    if (!read_headers(headers)) {
        if (on_log) on_log("Failed to read headers from " + get_client_ip(fd_));
        close(fd_);
        return;
    }

    std::istringstream rs(headers);
    std::string method, path, ver;
    rs >> method >> path >> ver;

    if (on_log) on_log("Request from " + get_client_ip(fd_) + ": " + method + " " + path);

    long long content_len = extract_content_length(headers);
    if (opts_.max_size > 0 && content_len > 0 && content_len > opts_.max_size) {
        if (on_log) on_log("Content length exceeds max size from " + get_client_ip(fd_));
        send_error(413, "413 Payload Too Large");
        close(fd_);
        return;
    }

    if (method == "GET") {
        handle_get_request(path, headers);
    } else if (method == "POST") {
        handle_post_request(path, headers);
    } else {
        send_error(405, "405 Method Not Allowed");
    }

    close(fd_);
    if (on_log) on_log("Client connection closed: " + get_client_ip(fd_));
}
