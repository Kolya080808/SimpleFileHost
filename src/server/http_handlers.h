#ifndef HTTP_HANDLERS_H
#define HTTP_HANDLERS_H

#include <string>

std::string html_upload_page(const std::string &token);
std::string html_download_page(const std::string &token, const std::string &filename, bool can_preview);

#endif
