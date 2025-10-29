#ifndef FILE_TRANSFER_H
#define FILE_TRANSFER_H

#include <string>
#include <atomic>


#include <openssl/ssl.h>


bool stream_file(int fd, const std::string& filepath, const std::string& content_type,
                const std::string& filename = "", bool as_attachment = false, 
                std::atomic<bool>* interrupted = nullptr, int timeout_seconds = 30, SSL* ssl = nullptr);

bool stream_receive_file(int fd, long long content_length, const std::string& boundary,
                        const std::string& outname, long long max_size, 
                        std::atomic<bool>* interrupted = nullptr, int timeout_seconds = 30, SSL* ssl = nullptr);

std::string format_size(long long bytes);
int calculate_percentage(long long current, long long total);

#endif
