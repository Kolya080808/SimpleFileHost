#pragma once
#include <string>

int create_zip_from_dir(const std::string &src_dir, const std::string &out_zip);
int create_zip_from_file(const std::string &file_path, const std::string &out_zip);

