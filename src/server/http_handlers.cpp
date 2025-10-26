#include "http_handlers.h"
#include <sstream>
#include <string>
#include <iomanip>

static std::string html_escape(const std::string &in) {
    std::ostringstream out;
    for (char c : in) {
        switch (c) {
            case '&': out << "&amp;"; break;
            case '<': out << "&lt;"; break;
            case '>': out << "&gt;"; break;
            case '"': out << "&quot;"; break;
            case '\'': out << "&#x27;"; break;
            case '/': out << "&#x2F;"; break;
            default: out << c; break;
        }
    }
    return out.str();
}

std::string html_upload_page(const std::string &token) {
    std::ostringstream s;
    s << "<!doctype html><html><head><meta charset=\"utf-8\"><title>Upload to SimpleFileHost</title>"
      << "<style>body{font-family:Arial;padding:20px} #drop{border:2px dashed #888;padding:30px;text-align:center}</style>"
      << "</head><body><h2>Upload file</h2>"
      << "<div id=\"drop\">Drag & Drop or use the form below<br><form method=\"POST\" enctype=\"multipart/form-data\">"
      << "<input type=\"file\" name=\"file\"><br><br><input type=\"submit\" value=\"Upload\"></form></div>"
      << "<script>var d=document.getElementById('drop'); d.ondragover=function(e){e.preventDefault();};"
      << "d.ondrop=function(e){e.preventDefault(); var fd=new FormData(); fd.append('file', e.dataTransfer.files[0]);"
      << "fetch('',{method:'POST', body:fd}).then(r=>r.text()).then(t=>document.body.innerHTML=t); };</script>"
      << "</body></html>";
    return s.str();
}

std::string html_download_page(const std::string &token, const std::string &filename, bool can_preview) {
    std::ostringstream s;
    std::string esc_name = html_escape(filename);
    s << "<!doctype html><html><head><meta charset=\"utf-8\"><title>Download</title></head><body>"
      << "<h2>File: " << esc_name << "</h2>";
    if (can_preview) {
        s << "<pre id=\"content\">(preview loading...)</pre>"
          << "<script>fetch('/" << token
          << "/raw').then(function(r){ return r.text(); }).then(function(t){ document.getElementById('content').textContent = t; })"
          << ".catch(function(){ document.getElementById('content').textContent = '(preview unavailable)'; });</script>";
    }
    s << "<a href='/" << token << "/file' download=\"" << esc_name << "\">Download</a>";
    s << "</body></html>";
    return s.str();
}

