#include "archive_utils.h"
#include "../utils/utils.h"
#include <archive.h>
#include <archive_entry.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <string>
#include <iostream>
#include <filesystem>
#include <vector>
#include <system_error>
#include <cerrno>
#include <limits.h>

namespace fs = std::filesystem;

static int add_file_to_archive(struct archive *a, const fs::path &file_path, const fs::path &base_path, const std::string &root_folder = "") {
    struct archive_entry *entry = archive_entry_new();
    if (!entry) return -1;

    std::string fullpath = file_path.string();
    std::string relpath;
    try {
        relpath = fs::relative(file_path, base_path).string();
    } catch (const std::exception &) {
        relpath = file_path.filename().string();
    }
    for (auto &c : relpath) if (c == '\\') c = '/';

    std::string pathname = relpath;
    if (!root_folder.empty()) {
        pathname = root_folder + "/" + relpath;
    }

    bool is_dir = fs::is_directory(file_path);
    if (is_dir) {
        if (pathname.empty() || pathname.back() != '/') pathname += '/';
    }

    archive_entry_set_pathname(entry, pathname.c_str());

    struct stat st;
    if (lstat(fullpath.c_str(), &st) != 0) {
        archive_entry_free(entry);
        if (is_verbose()) elog(std::string("[archive] lstat failed for ") + fullpath);
        return -2;
    }

    if (S_ISREG(st.st_mode)) {
        archive_entry_set_size(entry, st.st_size);
    } else {
        archive_entry_set_size(entry, 0);
    }

    archive_entry_set_filetype(entry, st.st_mode & S_IFMT);
    archive_entry_set_perm(entry, st.st_mode & 0777);

    if (S_ISLNK(st.st_mode)) {
        std::vector<char> linkbuf(PATH_MAX + 1);
        ssize_t r = readlink(fullpath.c_str(), linkbuf.data(), PATH_MAX);
        if (r > 0) {
            linkbuf[r] = '\0';
            archive_entry_set_symlink(entry, linkbuf.data());
        }
    }

    if (archive_write_header(a, entry) != ARCHIVE_OK) {
        if (is_verbose()) elog(std::string("[archive] Warning: failed to write header for ") + fullpath + ": " + archive_error_string(a));
    } else {
        if (S_ISREG(st.st_mode)) {
            FILE *f = fopen(fullpath.c_str(), "rb");
            if (f) {
                const size_t bufsize = 8192;
                std::vector<char> buf(bufsize);
                size_t r;
                while ((r = fread(buf.data(), 1, bufsize, f)) > 0) {
                    ssize_t w = archive_write_data(a, buf.data(), static_cast<size_t>(r));
                    if (w < 0) {
                        if (is_verbose()) elog(std::string("[archive] Error writing data for ") + fullpath + ": " + archive_error_string(a));
                        break;
                    }
                }
                fclose(f);
            } else {
                if (is_verbose()) elog(std::string("[archive] Warning: cannot open file ") + fullpath + " for reading");
            }
        }
    }

    archive_entry_free(entry);
    return 0;
}

int create_zip_from_dir(const std::string &src_dir, const std::string &out_zip) {
    try {
        fs::path base(src_dir);
        if (!fs::exists(base)) {
            std::cerr << "[archive] Source directory does not exist: " << src_dir << "\n";
            return 2;
        }

        std::string folder_name = base.filename().string();
        if (folder_name.empty()) {
            folder_name = base.parent_path().filename().string();
            if (folder_name.empty()) {
                folder_name = "archive";
            }
        }

        struct archive *a = archive_write_new();
        if (!a) return 3;

        archive_write_set_format_zip(a);
#if defined(ARCHIVE_VERSION_NUMBER) && ARCHIVE_VERSION_NUMBER >= 3000000
        archive_write_zip_set_compression_deflate(a);
#else
        archive_write_set_compression_deflate(a);
#endif
        archive_write_set_option(a, "zip", "compression-level", "9");

        if (archive_write_open_filename(a, out_zip.c_str()) != ARCHIVE_OK) {
            std::cerr << "[archive] Cannot open output zip: " << archive_error_string(a) << "\n";
            archive_write_free(a);
            return 4;
        }

        for (auto it = fs::recursive_directory_iterator(base); it != fs::recursive_directory_iterator(); ++it) {
            try {
                add_file_to_archive(a, it->path(), base, folder_name);
            } catch (const std::exception &ex) {
                if (is_verbose()) elog(std::string("[archive] Exception while adding: ") + it->path().string() + " : " + ex.what());
            }
        }

        add_file_to_archive(a, base, base, folder_name);

        if (archive_write_close(a) != ARCHIVE_OK) {
            if (is_verbose()) elog(std::string("[archive] Warning: error on close: ") + archive_error_string(a));
        }
        archive_write_free(a);
        return 0;
    } catch (const std::system_error &se) {
        std::cerr << "[archive] System error: " << se.what() << "\n";
        return 11;
    } catch (const std::exception &ex) {
        std::cerr << "[archive] Exception: " << ex.what() << "\n";
        return 10;
    } catch (...) {
        std::cerr << "[archive] Unknown exception\n";
        return 12;
    }
}

int create_zip_from_file(const std::string &file_path, const std::string &out_zip) {
    try {
        fs::path fpath(file_path);
        if (!fs::exists(fpath) || !fs::is_regular_file(fpath)) {
            std::cerr << "[archive] Source file does not exist or is not a regular file: " << file_path << "\n";
            return 2;
        }

        struct archive *a = archive_write_new();
        if (!a) return 3;

        archive_write_set_format_zip(a);
#if defined(ARCHIVE_VERSION_NUMBER) && ARCHIVE_VERSION_NUMBER >= 3000000
        archive_write_zip_set_compression_deflate(a);
#else
        archive_write_set_compression_deflate(a);
#endif
        archive_write_set_option(a, "zip", "compression-level", "9");

        if (archive_write_open_filename(a, out_zip.c_str()) != ARCHIVE_OK) {
            std::cerr << "[archive] Cannot open output zip: " << archive_error_string(a) << "\n";
            archive_write_free(a);
            return 4;
        }

        struct archive_entry *entry = archive_entry_new();
        std::string filename = fpath.filename().string();
        archive_entry_set_pathname(entry, filename.c_str());

        struct stat st;
        if (lstat(fpath.string().c_str(), &st) != 0) {
            archive_entry_free(entry);
            std::cerr << "[archive] lstat failed for " << fpath.string() << "\n";
            archive_write_free(a);
            return 5;
        }

        archive_entry_set_size(entry, st.st_size);
        archive_entry_set_filetype(entry, st.st_mode & S_IFMT);
        archive_entry_set_perm(entry, st.st_mode & 0777);

        if (archive_write_header(a, entry) != ARCHIVE_OK) {
            std::cerr << "[archive] Failed to write header for " << fpath.string() << ": " << archive_error_string(a) << "\n";
            archive_entry_free(entry);
            archive_write_free(a);
            return 6;
        }

        FILE *fp = fopen(fpath.string().c_str(), "rb");
        if (!fp) {
            std::cerr << "[archive] Cannot open file for reading: " << fpath.string() << "\n";
            archive_entry_free(entry);
            archive_write_free(a);
            return 7;
        }
        const size_t bufsize = 8192;
        std::vector<char> buf(bufsize);
        size_t r;
        while ((r = fread(buf.data(), 1, bufsize, fp)) > 0) {
            ssize_t w = archive_write_data(a, buf.data(), static_cast<size_t>(r));
            if (w < 0) {
                std::cerr << "[archive] Error writing data for " << fpath.string() << ": " + std::string(archive_error_string(a)) << "\n";
                fclose(fp);
                archive_entry_free(entry);
                archive_write_free(a);
                return 8;
            }
        }
        fclose(fp);

        archive_entry_free(entry);
        if (archive_write_close(a) != ARCHIVE_OK) {
            if (is_verbose()) elog(std::string("[archive] Warning: error on close: ") + archive_error_string(a));
        }
        archive_write_free(a);
        return 0;
    } catch (const std::system_error &se) {
        std::cerr << "[archive] System error: " << se.what() << "\n";
        return 11;
    } catch (const std::exception &ex) {
        std::cerr << "[archive] Exception: " << ex.what() << "\n";
        return 10;
    } catch (...) {
        std::cerr << "[archive] Unknown exception\n";
        return 12;
    }
}
