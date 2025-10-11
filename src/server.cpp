#include "server.h"
#include "utils.h"
#include <thread>
#include <vector>
#include <cstring>
#include <sstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>

static std::string html_upload_page(const std::string &token){
    std::ostringstream s;
    s<<"<!doctype html><html><head><meta charset=\"utf-8\"><title>Upload to SimpleFileHost</title>"
     <<"<style>body{font-family:Arial;padding:20px} #drop{border:2px dashed #888;padding:30px;text-align:center}</style>"
     <<"</head><body><h2>Upload file</h2>"
     <<"<div id=\"drop\">Drag & Drop or use the form below<br><form method=\"POST\" enctype=\"multipart/form-data\">"
     <<"<input type=\"file\" name=\"file\"><br><br><input type=\"submit\" value=\"Upload\"></form></div>"
     <<"<script>var d=document.getElementById('drop'); d.ondragover=function(e){e.preventDefault();}; d.ondrop=function(e){e.preventDefault(); var fd=new FormData(); fd.append('file', e.dataTransfer.files[0]); fetch('',{method:'POST', body:fd}).then(r=>r.text()).then(t=>document.body.innerHTML=t); };</script>"
     <<"</body></html>";
    return s.str();
}

static std::string html_download_page(const std::string &token, const std::string &filename, bool can_preview){
    std::ostringstream s;
    s<<"<!doctype html><html><head><meta charset=\"utf-8\"><title>Download</title></head><body>"
     <<"<h2>File: "<<filename<<"</h2>";
    if(can_preview){
        s<<"<pre id=\"content\">(preview loading...)</pre>"
         <<"<script>fetch('/"<<token<<"/raw').then(r=>r.text()).then(t=>document.getElementById('content').textContent=t);</script>";
    }
    s<<"<a href='/"<<token<<"/file' download>Download</a>";
    s<<"</body></html>";
    return s.str();
}

SimpleHTTPServer::SimpleHTTPServer(const ServerOptions& opt): opts(opt), port(opt.port) {}
SimpleHTTPServer::~SimpleHTTPServer(){ stop(); }

bool SimpleHTTPServer::start(){
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(listen_fd<0) return false;
    int optv=1; setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optv, sizeof(optv));
    sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_addr.s_addr=INADDR_ANY; addr.sin_port=htons(port);
    if(bind(listen_fd,(sockaddr*)&addr,sizeof(addr))<0) return false;
    if(port==0){
        socklen_t len = sizeof(addr);
        if(getsockname(listen_fd,(sockaddr*)&addr,&len)==0) port = ntohs(addr.sin_port);
    }
    if(listen(listen_fd, 5)<0) return false;
    running = true;
    std::thread(&SimpleHTTPServer::server_loop, this).detach();
    if(on_log) on_log("Server started on port " + std::to_string(port));
    return true;
}

void SimpleHTTPServer::stop(){
    if(running){
        running = false;
        if(listen_fd!=-1){ close(listen_fd); listen_fd=-1; }
    }
}

std::string SimpleHTTPServer::host_url() const {
    std::string ip = get_local_ip();
    std::ostringstream s;
    s << "http://" << ip << ":" << port << "/" << opts.token;
    return s.str();
}

void SimpleHTTPServer::server_loop(){
    while(running){
        sockaddr_in cli{}; socklen_t clilen = sizeof(cli);
        int fd = accept(listen_fd, (sockaddr*)&cli, &clilen);
        if(fd<0) break;

        if(on_log){
            std::ostringstream ss; ss<<"Client connected: "<<inet_ntoa(cli.sin_addr)<<":"<<ntohs(cli.sin_port);
            on_log(ss.str());
        }

        std::string req;
        char chunk[4096];
        ssize_t r;
        while ((r = recv(fd, chunk, sizeof(chunk), 0)) > 0) {
            req.append(chunk, r);
            if (req.find("\r\n\r\n") != std::string::npos && req.rfind("POST", 0) != 0)
                break;
            auto pos = req.find("Content-Length:");
            if (pos != std::string::npos) {
                pos += strlen("Content-Length:");
                while (pos < req.size() && req[pos] == ' ') pos++;
                size_t end = req.find("\r\n", pos);
                if (end != std::string::npos) {
                    long long len = std::stoll(req.substr(pos, end - pos));
                    size_t header_end = req.find("\r\n\r\n");
                    if (header_end != std::string::npos &&
                        req.size() >= header_end + 4 + len)
                        break;
                }
            }
        }
        if (r < 0 || req.empty()) { close(fd); continue; }

        std::istringstream rs(req);
        std::string method, path, ver;
        rs >> method >> path >> ver;

        if(method=="GET"){
            if(path=="/"+opts.token){
                if(opts.mode=="get"){
                    auto html = html_upload_page(opts.token);
                    std::ostringstream resp;
                    resp<<"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: "<<html.size()<<"\r\n\r\n"<<html;
                    send(fd, resp.str().c_str(), resp.str().size(), 0);
                } else {
                    bool can_preview = (mime_type(opts.path).rfind("text/",0)==0);
                    auto html = html_download_page(opts.token, file_basename(opts.path), can_preview);
                    std::ostringstream resp;
                    resp<<"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: "<<html.size()<<"\r\n\r\n"<<html;
                    send(fd, resp.str().c_str(), resp.str().size(), 0);
                }
            }
            else if(path=="/"+opts.token+"/file" && opts.mode=="send"){
                std::string data = read_file_all(opts.path);
                std::string filename = file_basename(opts.path);
                std::ostringstream resp;
                resp << "HTTP/1.1 200 OK\r\n"
                     << "Content-Type: " << mime_type(opts.path) << "\r\n"
                     << "Content-Length: " << data.size() << "\r\n"
                     << "Content-Disposition: attachment; filename=\"" << filename << "\"\r\n"
                     << "\r\n";
                send(fd, resp.str().c_str(), resp.str().size(), 0);
                send(fd, data.data(), data.size(), 0);
                if(on_log) on_log("File served to client: " + filename);
                if(on_client_done) on_client_done();
            }
            else if(path=="/"+opts.token+"/raw" && opts.mode=="send"){
                std::string data = read_file_all(opts.path);
                std::ostringstream resp;
                resp<<"HTTP/1.1 200 OK\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: "<<data.size()<<"\r\n\r\n";
                send(fd, resp.str().c_str(), resp.str().size(), 0);
                send(fd, data.data(), data.size(), 0);
            }
            else {
                std::string notfound = "404 Not Found";
                std::ostringstream resp;
                resp<<"HTTP/1.1 404 Not Found\r\nContent-Length: "<<notfound.size()<<"\r\n\r\n"<<notfound;
                send(fd, resp.str().c_str(), resp.str().size(), 0);
            }
        }
        else if(method=="POST" && opts.mode=="get"){
            auto pos = req.find("Content-Type: multipart/form-data; boundary=");
            if(pos!=std::string::npos){
                auto bstart = pos + strlen("Content-Type: multipart/form-data; boundary=");
                std::string boundary;
                for(size_t i=bstart;i<req.size() && req[i]!='\r' && req[i]!='\n';++i) boundary.push_back(req[i]);
                std::string marker = "--" + boundary;

                auto fpos = req.find(marker);
                if(fpos!=std::string::npos){
                    auto namepos = req.find("filename=\"", fpos);
                    std::string fname;
                    if(namepos!=std::string::npos){
                        namepos += strlen("filename=\"");
                        size_t endq = req.find('"', namepos);
                        if(endq!=std::string::npos){
                            fname = req.substr(namepos, endq - namepos);
                        }
                    }

                    if (opts.path.empty() || opts.path == "." || opts.path == "./") {
                        fname = file_basename(fname);
                    } else if (file_exists(opts.path) && opts.path.back() == '/') {
                        fname = opts.path + file_basename(fname);
                    } else {
                        if(fname.empty()) fname = opts.path;
                    }

                    auto data_start = req.find("\r\n\r\n", fpos);
                    if(data_start!=std::string::npos){
                        data_start += 4;
                        auto data_end = req.find("\r\n" + marker, data_start);
                        if(data_end==std::string::npos) data_end = req.size();
                        std::string filedata = req.substr(data_start, data_end - data_start);
                        write_file(fname, filedata);
                        if(on_log) on_log("Received upload: " + fname + " (" + std::to_string(filedata.size()) + " bytes)");
                        std::string ok = "<html><body>Upload saved. You can close this page.</body></html>";
                        std::ostringstream resp;
                        resp<<"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: "<<ok.size()<<"\r\n\r\n"<<ok;
                        send(fd, resp.str().c_str(), resp.str().size(), 0);
                        if(on_client_done) on_client_done();
                    }
                }
            }
        }
        else {
            std::string bad = "501 Not Implemented";
            std::ostringstream resp;
            resp<<"HTTP/1.1 501 Not Implemented\r\nContent-Length: "<<bad.size()<<"\r\n\r\n"<<bad;
            send(fd, resp.str().c_str(), resp.str().size(), 0);
        }
        close(fd);
    }
    running=false;
}

