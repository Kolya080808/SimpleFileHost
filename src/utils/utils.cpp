#include "utils.h"
#include <atomic>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <cctype>
#include <algorithm>
#include <cstdlib>
#include <ctime>

static std::atomic<bool> g_verbose{false};

void set_verbose(bool v) {
    g_verbose = v;
}

bool is_verbose() {
    return g_verbose.load();
}

void vlog(const std::string &m) {
    if (is_verbose()) {
        std::cerr << "[INFO] " << m << std::endl;
    }
}

void elog(const std::string &m) {
    std::cerr << "[ERROR] " << m << std::endl;
}

static const char TOKEN_ALPHABET[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789";

std::string random_token(size_t n) {
    if (n == 0) return "";

    const size_t alphabet_size = sizeof(TOKEN_ALPHABET) - 1;
    std::string out;
    out.reserve(n);

    std::ifstream urand("/dev/urandom", std::ios::in | std::ios::binary);
    if (urand.is_open()) {
        const unsigned int max_acceptable = (256u / alphabet_size) * alphabet_size;
        while (out.size() < n && urand.good()) {
            unsigned char byte = 0;
            urand.read(reinterpret_cast<char*>(&byte), 1);
            if (!urand) break;
            if (byte >= max_acceptable) continue;
            unsigned int idx = byte % alphabet_size;
            out.push_back(TOKEN_ALPHABET[idx]);
        }
    }

    if (out.size() < n) {
        vlog("Warning: filling token with fallback PRNG");
        std::srand(static_cast<unsigned int>(std::time(nullptr) ^ reinterpret_cast<uintptr_t>(&out)));
        while (out.size() < n) {
            out.push_back(TOKEN_ALPHABET[std::rand() % alphabet_size]);
        }
    }

    return out;
}

long long parse_size(const std::string &s) {
    if (s.empty()) return 0;
    std::string t;
    for (char c : s) {
        if (!isspace((unsigned char)c)) t.push_back((char)tolower((unsigned char)c));
    }

    long long mult = 1;
    if (t.size() > 2) {
        std::string suf = t.substr(t.size() - 2);
        if (suf == "kb") {
            mult = 1024LL;
            t = t.substr(0, t.size() - 2);
        } else if (suf == "mb") {
            mult = 1024LL * 1024LL;
            t = t.substr(0, t.size() - 2);
        } else if (suf == "gb") {
            mult = 1024LL * 1024LL * 1024LL;
            t = t.substr(0, t.size() - 2);
        } 
    } else if (t.size() > 1) {
        char last = t.back();
        if (last == 'k') { mult = 1024LL; t.pop_back(); }
        else if (last == 'm') { mult = 1024LL * 1024LL; t.pop_back(); }
        else if (last == 'g') { mult = 1024LL * 1024LL * 1024LL; t.pop_back(); }
    }

    try {
        long long v = std::stoll(t);
        return v * mult;
    } catch (...) {
        return 0;
    }
}

