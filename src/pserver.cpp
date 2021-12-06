#include <cstdio>
#include <sstream>
#include <thread>
#include <iostream>
#include <regex>

#include "proxyserver.h"
#include "logger.h"

void print_help()
{
    printf("pserver [--name=<local name>] [--port=<local port>]\n");
}

int main(int argc, char **argv)
{
    start_logger(stdout);

    const std::regex rgx("--(.+):(.+)");
    int lport = 8001;
    std::string lname ("0.0.0.0");

    std::cmatch cm;
    for (int i = 1; i < argc; ++i)
    {
        std::regex_match(argv[i], cm, rgx);
        if (cm.size() < 3)
        {
            fprintf(stderr, "unkown option: '%s'\n", argv[i]);
            print_help();
            exit(1);
        }

        std::string opt {cm.str(1)};
        std::string val {cm.str(2)};

        if (opt == "lname") {
            lname = val;
        } else if (opt == "lport") {
            lport = atoi(val.c_str());
        } else {
            fprintf(stderr, "unkown option: '%s'\n", argv[i]);
            print_help();
            exit(1);
        }
    }

    Server s(lname, lport);
    s.listen();

    return 0;
}
