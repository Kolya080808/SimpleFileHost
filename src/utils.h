#ifndef SIMPLEFILEHOST_UTILS_H
#define SIMPLEFILEHOST_UTILS_H

#include <string>

std::string random_token(size_t len = 20);
bool file_exists(const std::string &path);
std::string file_basename(const std::string &path);
std::string mime_type(const std::string &name);
void write_file(const std::string &path, const std::string &data);
std::string read_file_all(const std::string &path);
std::string get_local_ip();

#endif

