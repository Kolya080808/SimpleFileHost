#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <regex>
#include <sstream>
#include <unistd.h>
#include <libgen.h>
#include <fstream>
#include <limits.h>

#include "cli.h"
#include "server/server.h"
#include "utils/archive_utils.h"
#include "utils/utils.h"
#include "utils/file_utils.h"
#include "utils/network_utils.h"
#include "qr/qr_display.h"

#ifdef HAVE_QRENCODE
#include <qrencode.h>
#endif

#include <filesystem>
namespace fs = std::filesystem;

static std::atomic<bool> server_finished{false};
std::atomic<bool> interrupted{false};

void handle_sigint(int) {
    interrupted = true;
    std::cout << "\n[!] Operation cancelled.\n> " << std::flush;
}

long long parse_size_local(const std::string &s){
    return parse_size(s);
}

static long long get_env_max_size_bytes() {
    const char *e = std::getenv("SIMPLEFILEHOST_MAX_SIZE");
    if (!e) return 0;
    try {
        long long v = std::stoll(std::string(e));
        return v >= 0 ? v : 0;
    } catch (...) {
        return 0;
    }
}

void run_send(const std::string &filepath){
    if(!file_exists(filepath)){
        std::cerr << "File not found: " << filepath << "\n";
        return;
    }

    std::ifstream test_file(filepath, std::ios::binary);
    if (!test_file.is_open()) {
        std::cerr << "Cannot read file: " << filepath << " (permission denied?)\n";
        return;
    }
    test_file.close();

    ServerOptions opt;
    opt.mode = "send";
    opt.token = random_token(24);
    opt.path = filepath;
    opt.bind_address = get_default_bind_address();
    opt.max_size = get_env_max_size_bytes();
    opt.interrupted = &interrupted;
    opt.socket_timeout_seconds = 60;

    char filepath_abs[PATH_MAX];
    if (realpath(filepath.c_str(), filepath_abs) != nullptr) {
        std::string dir_path = filepath_abs;
        size_t last_slash = dir_path.find_last_of('/');
        if (last_slash != std::string::npos) {
            dir_path = dir_path.substr(0, last_slash);
            if (dir_path.empty()) {
                dir_path = "/";
            }
        } else {
            dir_path = ".";
        }
        opt.working_dir = dir_path;
        std::cout << "[INFO] Working directory set to file location: " << dir_path << std::endl;
    } else {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd))) {
            opt.working_dir = cwd;
            std::cout << "[INFO] Working directory set to current dir: " << cwd << std::endl;
        } else {
            opt.working_dir = ".";
        }
    }

    SimpleHTTPServer srv(opt);

    srv.on_log = [](const std::string &m){ std::cout << "[srv] " << m << "\n"; };
    srv.on_client_done = [&](){
        std::cout << "[srv] Transfer complete, shutting down.\n";
        server_finished = true;
        srv.stop();
    };

    if(!srv.start()){
        std::cerr << "Failed to start server\n";
        return;
    }

    std::string uri = srv.host_url();
    std::cout << "Open this URL on the receiver device:\n";
    print_qr_ascii(uri);
    std::cout << "Waiting for client to download... Press Ctrl-C to cancel.\n";

    while(!server_finished && !interrupted)
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

    if (interrupted) {
        interrupted = false;
        std::cout << "[srv] Cancelled by user.\n";
        srv.stop();
        return;
    }
}

void run_get(const std::string &outfile){
    ServerOptions opt;
    opt.mode = "get";
    opt.token = random_token(24);
    opt.path = outfile;
    opt.bind_address = get_default_bind_address();
    opt.max_size = get_env_max_size_bytes();
    opt.interrupted = &interrupted;
    opt.socket_timeout_seconds = 60;

    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) {
        opt.working_dir = cwd;
        std::cout << "[INFO] Working directory set to current dir: " << cwd << std::endl;
    } else {
        opt.working_dir = ".";
    }

    SimpleHTTPServer srv(opt);
    srv.on_log = [](const std::string &m){ std::cout << "[srv] " << m << "\n"; };
    srv.on_client_done = [&](){
        std::cout << "[srv] Upload received, shutting down.\n";
        server_finished = true;
        srv.stop();
    };
    if(!srv.start()){ std::cerr << "Failed to start server\n"; return; }
    std::string uri = srv.host_url();
    std::cout << "Open this URL on the sender device and upload the file:\n";
    print_qr_ascii(uri);
    std::cout << "Waiting for upload... Press Ctrl-C to cancel.\n";
    while(!server_finished && !interrupted)
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

    if (interrupted) {
        interrupted = false;
        std::cout << "[srv] Cancelled by user.\n";
        srv.stop();
        return;
    }
}

void run_zip(const std::string &target, const std::string &split_arg = ""){
    fs::path p(target);
    if (!fs::exists(p)) {
        std::cerr << "zip failed: target does not exist: " << target << "\n";
        return;
    }

    char cwd[1024];
    if (!getcwd(cwd, sizeof(cwd))) {
        std::cerr << "zip failed: cannot get current working directory\n";
        return;
    }

    std::string basename = p.filename().string();
    if (basename.empty()) {
        basename = p.parent_path().filename().string();
        if (basename.empty()) {
            basename = "archive";
        }
    }
    std::string out = std::string(cwd) + "/" + basename + ".zip";

    int rc = 1;
    if (fs::is_regular_file(p)) {
        rc = create_zip_from_file(target, out);
    } else if (fs::is_directory(p)) {
        rc = create_zip_from_dir(target, out);
    } else {
        std::cerr << "zip failed: target is not a regular file or directory: " << target << "\n";
        return;
    }

    if(rc != 0){
        std::cerr << "zip failed (libarchive returned " << rc << ")\n";
        return;
    }
    std::cout << "[zip] Created archive: " << out << "\n";
}

void print_help(){
    std::cout << "\nAvailable commands:\n"
              << "  send <file>              — Send file over Wi-Fi.\n"
              << "  senddir <dir>            — Send entire folder (auto zipped).\n"
              << "  get <output_file>        — Receive file from another device.\n"
              << "  zip <target>             — Archive.\n"
              << "  help                     — Show this help message.\n"
              << "  exit                     — Quit program.\n"
              << std::endl;
}

void repl(){
    signal(SIGINT, handle_sigint);
    std::string line;
    while(true){
        // Сбросим флаги в начале каждой итерации REPL,
        // чтобы предыдущая сессия не мешала новой.
        interrupted.store(false);
        server_finished = false;

        std::cout << "> ";
        if(!std::getline(std::cin, line)) break;
        if(line.empty()) continue;

        if(line == "exit") break;
        else if(line.rfind("send ", 0) == 0){
            std::string f = line.substr(5);
            run_send(f);
            server_finished = false;
            interrupted = false;
        }
        else if(line == "get"){
            run_get("");
            server_finished = false;
            interrupted = false;
        }
        else if(line.rfind("get ", 0) == 0){
            std::string out = line.substr(4);
            run_get(out);
            server_finished = false;
            interrupted = false;
        }

        else if(line.rfind("zip ", 0) == 0){
            std::istringstream ss(line);
            std::string cmd, target, size;
            ss >> cmd >> target >> size;
            if(target.empty()){
                std::cout << "Usage: zip <target> [split_size<kb/mb/gb>]\n";
                continue;
            }
            run_zip(target, size);
            interrupted = false;
        }
        else if(line.rfind("senddir ", 0) == 0){
            std::string d = line.substr(8);
            char cwd[1024];
            if (!getcwd(cwd, sizeof(cwd))) {
                std::cerr << "senddir failed: cannot get current working directory\n";
                continue;
            }

            fs::path p(d);
            std::string basename = p.filename().string();
            if (basename.empty()) {
                basename = p.parent_path().filename().string();
                if (basename.empty()) {
                    basename = "archive";
                }
            }
            std::string temp_zip = std::string(cwd) + "/" + basename + ".zip";
            std::cout << "[*] Archiving directory: " << d << " to " << temp_zip << "\n";

            if (create_zip_from_dir(d, temp_zip) != 0) {
                std::cerr << "Failed to create zip for directory: " << d << "\n";
            } else {
                std::string original_cwd = cwd;
                run_send(temp_zip);
                if (chdir(original_cwd.c_str()) != 0) {
                    std::cerr << "Warning: could not return to original directory\n";
                }
                if (std::remove(temp_zip.c_str()) == 0) {
                    std::cout << "[*] Temporary archive removed: " << temp_zip << "\n";
                } else {
                    std::cerr << "Warning: could not remove temporary archive: " << temp_zip << "\n";
                }
            }

            server_finished = false;
            interrupted = false;
        }
        else if(line == "help"){
            print_help();
            interrupted = false;
        }
        else {
            std::cout << "Unknown command. Type 'help' for help.\n";
            interrupted = false;
        }
    }
}

