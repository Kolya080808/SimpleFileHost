#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <string>

bool file_exists(const std::string &path);
std::string file_basename(const std::string &path);
std::string mime_type(const std::string &name);
void write_file(const std::string &path, const std::string &data);
std::string read_file_all(const std::string &path);

#endif
