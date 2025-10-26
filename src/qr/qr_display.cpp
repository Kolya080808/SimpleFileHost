#include "qr_display.h"
#include <iostream>

#ifdef HAVE_QRENCODE
#include <qrencode.h>
#endif

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
