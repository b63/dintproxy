#include <cstdio>
#include <sstream>
#include <thread>
#include <iostream>
#include <regex>

#include "proxyclient.h"
#include "logger.h"

void print_help()
{
    printf("pclient [--lname=<local name>] [--lport=<local port>] \n\
    [--sname=<server name>] [--sport=<server port>]\n");
}

int main(int argc, char **argv)
{
    start_logger(stdout);

    const std::regex rgx("--(.+):(.+)");
    int lport = 5001, rport = 8001;
    std::string lname ("0.0.0.0");
    std::string rname ("0.0.0.0");

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
        } else if (opt == "rname") {
            rname = val;
        } else if (opt == "lport") {
            lport = atoi(val.c_str());
        } else if (opt == "rport") {
            rport = atoi(val.c_str());
        } else {
            fprintf(stderr, "unkown option: '%s'\n", argv[i]);
            print_help();
            exit(1);
        }
    }

    Client c(lname, lport, rname, rport);
    c.listen();

    return 0;
}
