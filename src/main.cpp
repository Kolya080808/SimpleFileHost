#include <iostream>
#include "utils.h"
#include "cli.h"

int main(int argc, char **argv){
    std::cout<<"SimpleFileHost — local file sharing over Wi-Fi\nType 'help' for commands.\n";
    repl();
    std::cout<<"Goodbye.\n";
    return 0;
}
