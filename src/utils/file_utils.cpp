#include "file_utils.h"
#include <sys/stat.h>
#include <fstream>
#include <sstream>

bool file_exists(const std::string &path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

std::string file_basename(const std::string &path) {
    auto p = path.find_last_of("/\\");
    if (p == std::string::npos) return path;
    return path.substr(p + 1);
}

std::string mime_type(const std::string &name) {
    if (name.find(".html") != std::string::npos) return "text/html";
    if (name.find(".htm") != std::string::npos) return "text/html";
    if (name.find(".txt") != std::string::npos) return "text/plain";
    if (name.find(".css") != std::string::npos) return "text/css";
    if (name.find(".js") != std::string::npos) return "application/javascript";
    if (name.find(".json") != std::string::npos) return "application/json";
    if (name.find(".png") != std::string::npos) return "image/png";
    if (name.find(".jpg") != std::string::npos || name.find(".jpeg") != std::string::npos) return "image/jpeg";
    if (name.find(".gif") != std::string::npos) return "image/gif";
    if (name.find(".svg") != std::string::npos) return "image/svg+xml";
    if (name.find(".zip") != std::string::npos) return "application/zip";
    return "application/octet-stream";
}

void write_file(const std::string &path, const std::string &data) {
    std::ofstream ofs(path, std::ios::binary);
    ofs.write(data.data(), data.size());
}

std::string read_file_all(const std::string &path) {
    std::ifstream ifs(path, std::ios::binary);
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}
