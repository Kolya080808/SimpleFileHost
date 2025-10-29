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

static bool g_tls_enabled = false;
static std::string g_tls_cert;
static std::string g_tls_key;
static std::mutex g_tls_mutex;

void set_tls_enabled(bool enabled) {
    std::lock_guard<std::mutex> lk(g_tls_mutex);
    g_tls_enabled = enabled;
}

bool get_tls_enabled() {
    std::lock_guard<std::mutex> lk(g_tls_mutex);
    return g_tls_enabled;
}

void set_tls_files(const std::string &cert_path, const std::string &key_path) {
    std::lock_guard<std::mutex> lk(g_tls_mutex);
    g_tls_cert = cert_path;
    g_tls_key = key_path;
}

std::string get_tls_cert() {
    std::lock_guard<std::mutex> lk(g_tls_mutex);
    return g_tls_cert;
}

std::string get_tls_key() {
    std::lock_guard<std::mutex> lk(g_tls_mutex);
    return g_tls_key;
}
