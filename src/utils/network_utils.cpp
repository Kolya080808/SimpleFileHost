#include "network_utils.h"
#include <mutex>

static std::string g_default_bind_address = "127.0.0.1";
static std::mutex g_bind_mutex;

void set_default_bind_address(const std::string &addr) {
    std::lock_guard<std::mutex> lk(g_bind_mutex);
    g_default_bind_address = addr;
}

std::string get_default_bind_address() {
    std::lock_guard<std::mutex> lk(g_bind_mutex);
    return g_default_bind_address;
}

