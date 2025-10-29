#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <csignal>
#include "utils/utils.h"
#include "utils/network_utils.h"
#include "cli/cli.h"

static const std::string VERSION = "2.0";

enum ExitCode {
    EXIT_SUCCESS_OK = 0,
    EXIT_GENERIC_ERROR = 1,
    EXIT_INVALID_ARGUMENT = 2,
    EXIT_NETWORK_ERROR = 3
};

void print_global_help() {
    std::cout <<
    "SimpleFileHost — lightweight local file sharing server\n"
    "\nUsage:\n"
    "  simplefilehost [options]\n"
    "\nOptions:\n"
    "  --bind <address>        Bind to specific IP address (e.g., 0.0.0.0)\n"
    "  --auto-bind             Bind to 0.0.0.0 (make server reachable on all interfaces)\n"
    "  --max-size <bytes>      Limit maximum upload size (e.g., 100MB)\n"
    "  --verbose               Enable detailed log output to stderr\n"
    "  --help                  Show this help message and exit\n"
    "  --version               Show program version and exit\n"
    "  --tls <cert> <key>      Enable TLS (HTTPS) using the provided certificate and private key files\n"
    << std::endl;
}

int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);
    
    std::string bind_address = "127.0.0.1";
    bool auto_bind = false;

    long long default_max_size = 200LL * 1024LL * 1024LL;
    long long max_size = default_max_size;

    std::string tls_cert_arg;
    std::string tls_key_arg;
    bool tls_enabled_arg = false;

    if (argc > 1) {
        std::vector<std::string> args(argv + 1, argv + argc);

        for (size_t i = 0; i < args.size(); ++i) {
            const std::string &a = args[i];

            if (a == "--help" || a == "-h") {
                print_global_help();
                return EXIT_SUCCESS_OK;
            }
            else if (a == "--version" || a == "-v") {
                std::cout << "SimpleFileHost version " << VERSION << std::endl;
                return EXIT_SUCCESS_OK;
            }
            else if (a == "--verbose") {
                set_verbose(true);
                vlog("Verbose mode enabled");
            }
            else if (a == "--auto-bind") {
                auto_bind = true;
                vlog("Auto-bind flag specified (will bind to 0.0.0.0 unless --bind is also provided)");
            }
            else if (a == "--bind") {
                if (i + 1 >= args.size()) {
                    elog("--bind requires an IP address argument");
                    return EXIT_INVALID_ARGUMENT;
                }
                bind_address = args[++i];
                vlog("Manual bind address: " + bind_address);
            }
            else if (a == "--max-size") {
                if (i + 1 >= args.size()) {
                    elog("--max-size requires an argument (e.g., 100mb)");
                    return EXIT_INVALID_ARGUMENT;
                }
                std::string s = args[++i];
                long long v = parse_size(s);
                if (v <= 0) {
                    elog("Invalid --max-size value: " + s);
                    return EXIT_INVALID_ARGUMENT;
                }
                max_size = v;
                vlog("Max upload size set to " + std::to_string(max_size) + " bytes");
            }
            else if (a == "--tls") {
                if (i + 2 >= args.size()) {
                    elog("--tls requires two arguments: <cert> <key>");
                    return EXIT_INVALID_ARGUMENT;
                }
                tls_cert_arg = args[++i];
                tls_key_arg = args[++i];
                tls_enabled_arg = true;
                vlog("TLS enabled with cert: " + tls_cert_arg + " key: " + tls_key_arg);
            }
            else if (a.rfind("--", 0) == 0) {
                elog("Unknown option: " + a);
                std::cerr << "Use --help for usage information." << std::endl;
                return EXIT_INVALID_ARGUMENT;
            }
        }
    }

    if (auto_bind) {
        if (bind_address == "127.0.0.1") {
            bind_address = "0.0.0.0";
            vlog("Auto-bind engaged: binding to 0.0.0.0");
        } else {
            vlog("--auto-bind specified but explicit --bind present; using explicit --bind: " + bind_address);
        }
    }

    set_default_bind_address(bind_address);

    if (tls_enabled_arg) {
        set_tls_enabled(true);
        set_tls_files(tls_cert_arg, tls_key_arg);
    } else {
        set_tls_enabled(false);
    }

    std::cout << "SimpleFileHost — local file sharing over Wi-Fi\n";
    std::cout << "Listening on: " << bind_address << std::endl;
    std::cout << "Type 'help' for interactive commands.\n";

    if (is_verbose()) {
        vlog("Default max-size: " + std::to_string(max_size) + " bytes");
    }

    std::string env_val = std::to_string(max_size);
    setenv("SIMPLEFILEHOST_MAX_SIZE", env_val.c_str(), 1);

    vlog("Starting REPL loop");
    try {
        repl();
    } catch (const std::exception &e) {
        elog(std::string("Runtime error: ") + e.what());
        return EXIT_GENERIC_ERROR;
    } catch (...) {
        elog("Unknown fatal error occurred");
        return EXIT_GENERIC_ERROR;
    }

    vlog("Exiting application");
    std::cout << "Goodbye.\n";
    return EXIT_SUCCESS_OK;
}
