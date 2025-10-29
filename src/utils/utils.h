#ifndef UTILS_H
#define UTILS_H

#include <string>

void set_verbose(bool v);
bool is_verbose();
void vlog(const std::string &m);
void elog(const std::string &m);

std::string random_token(size_t n);

long long parse_size(const std::string &s);

#endif
