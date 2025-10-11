#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <cstdlib>
#include <regex>
#include "server.h"
#include "utils.h"

#ifdef HAVE_QRENCODE
#include <qrencode.h>
#endif

static std::atomic<bool> server_finished{false};
std::atomic<bool> interrupted{false};

void handle_sigint(int) {
    interrupted = true;
    std::cout << "\n[!] Operation cancelled.\n> " << std::flush;
}

void print_boxed(const std::string &s){
    std::cout << "+" << std::string(s.size() + 2, '-') << "+\n";
    std::cout << "| " << s << " |\n";
    std::cout << "+" << std::string(s.size() + 2, '-') << "+\n";
}

void print_qr_ascii(const std::string &uri){
#ifdef HAVE_QRENCODE
    QRcode *q = QRcode_encodeString(uri.c_str(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    if(!q){ std::cout << uri << "\n"; return; }
    int w = q->width;
    for(int y = -1; y <= w; ++y){
        for(int x = -1; x <= w; ++x){
            if(x < 0 || y < 0 || x >= w || y >= w || (q->data[y*w + x] & 1))
                std::cout << "██";
            else
                std::cout << "  ";
        }
        std::cout << "\n";
    }
    QRcode_free(q);
#else
    print_boxed(uri);
#endif
    std::cout << "\nLink: " << uri << "\n";
}


long long parse_size(const std::string &s){
    std::regex re(R"(^(\d+)(kb|mb|gb)?$)", std::regex::icase);
    std::smatch m;
    if(!std::regex_match(s, m, re)) return -1;
    long long value = std::stoll(m[1].str());
    std::string unit = m[2].matched ? m[2].str() : "";
    for(auto &c: unit) c = std::tolower(c);
    if(unit == "kb") value *= 1024;
    else if(unit == "mb") value *= 1024 * 1024;
    else if(unit == "gb") value *= 1024LL * 1024LL * 1024LL;
    return value;
}


void run_send(const std::string &filepath){
    if(!file_exists(filepath)){ std::cerr << "File not found: " << filepath << "\n"; return; }
    ServerOptions opt;
    opt.mode = "send";
    opt.token = random_token(24);
    opt.path = filepath;
    SimpleHTTPServer srv(opt);
    srv.on_log = [](const std::string &m){ std::cout << "[srv] " << m << "\n"; };
    srv.on_client_done = [&](){
        std::cout << "[srv] Transfer complete, shutting down.\n";
        server_finished = true;
        srv.stop();
    };
    if(!srv.start()){ std::cerr << "Failed to start server\n"; return; }
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
    std::string out = target + ".zip";
    std::string zip_cmd = "zip -r \"" + out + "\" \"" + target + "\"";
    std::cout << "[*] Archiving: " << zip_cmd << "\n";
    int rc = system(zip_cmd.c_str());
    if(rc != 0){ std::cerr << "zip failed (ensure 'zip' is installed)\n"; return; }

    if(!split_arg.empty()){
        long long bytes = parse_size(split_arg);
        if(bytes <= 0){ std::cerr << "Invalid split size: " << split_arg << "\n"; return; }

        std::string size_str;
        if(bytes % (1024LL*1024LL*1024LL) == 0)
            size_str = std::to_string(bytes / (1024LL*1024LL*1024LL)) + "G";
        else if(bytes % (1024LL*1024LL) == 0)
            size_str = std::to_string(bytes / (1024LL*1024LL)) + "M";
        else if(bytes % 1024 == 0)
            size_str = std::to_string(bytes / 1024) + "K";
        else
            size_str = std::to_string(bytes);

        std::string split_cmd = "split -b " + size_str + " \"" + out + "\" \"" + out + ".part\"";
        std::cout << "[*] Splitting: " << split_cmd << "\n";
        rc = system(split_cmd.c_str());
        if(rc == 0)
            std::cout << "[ok] Split complete.\n";
        else
            std::cerr << "split failed (ensure 'split' is installed)\n";
    } else {
        std::cout << "[ok] Created " << out << "\n";
    }
}


void print_help(){
    std::cout << "\nAvailable commands:\n"
              << "  send <file>              — Send file over Wi-Fi.\n"
              << "  senddir <dir>            — Send entire folder (auto zipped).\n"
              << "  get <output_file>        — Receive file from another device.\n"
              << "  zip <target> [size]      — Archive and optionally split.\n"
              << "                             Example: zip hello 500mb\n"
              << "  help                     — Show this help message.\n"
              << "  exit                     — Quit program.\n"
              << "\nNotes:\n"
              << "  - Supported split units: kb, mb, gb.\n"
              << "  - Requires 'zip' and 'split' commands installed.\n"
              << std::endl;
}


void repl(){
    signal(SIGINT, handle_sigint);
    std::string line;
    while(true){
        std::cout << "> ";
        if(!std::getline(std::cin, line)) break;
        if(line.empty()) continue;

        if(line == "exit" || line == "quit") break;
        else if(line.rfind("send ", 0) == 0){
            std::string f = line.substr(5);
            run_send(f);
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
            std::string zipname = d + ".zip";
            std::string cmd = "zip -r \"" + zipname + "\" \"" + d + "\"";
            std::cout << "[*] Archiving: " << cmd << "\n";
            system(cmd.c_str());
            run_send(zipname);
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

