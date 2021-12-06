#include <string>
#include <memory>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <regex>
#include <sstream>
#include <filesystem>
#include <iostream>

#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h> 
#include <error.h>
#include <ctime>

#include <dint/dint_public.h>
#include <dint/exceptions.h>

#include "proxyserver.h"
#include "util.h"
#include "tcppipe.h"
#include "logger.h"


FILE *logfd = 0;
bool close_fd = false;
bool printbuf = false;
pid_t parentpid = 0;
const char *LOGDIR = "/tmp";

void close_logger()
{
    if (logfd!=nullptr)
        fclose(logfd);
    logfd = nullptr;
}

// questionable hack to ensure logfd is closed
class EOBJ
{
    public:
        ~EOBJ(){
            if (getpid() == parentpid && close_fd)
                close_logger();
        }
};

const EOBJ _obj{};

void buf_print_enable(bool state)
{
    printbuf = state;
}

void start_logger(const char *fp)
{
    std::stringstream ss;
    ss << LOGDIR << "/" << fp;
    std::string filep {ss.str()};

    logfd = fopen(filep.c_str(), "a");

    if (logfd == nullptr)
        throw dint::Exception("unable to open logfile");

    time_t t = time(0);
    fprintf(logfd, "---- LOG STARTING %s ---- \n", ctime(&t));
    fflush(logfd);
    parentpid = getpid();
    close_fd = true;
}


void start_logger(FILE *fp)
{
    logfd = fp;

    time_t t = time(0);
    fprintf(logfd, "---- LOG STARTING ----\n%s", ctime(&t));
    fflush(logfd);
    parentpid = getpid();
}

void _log_function(const char *fname, const char *color, FILE *f)
{
    if (f == nullptr)
        f = logfd;

    if (f == nullptr)
        throw dint::Exception("log file is not open");

    const char *start = strchr(fname, ' ');
    const char *end = strrchr(fname, ')');

    std::string fname_str (fname);
    if (fname_str.find("lambda") != std::string::npos){
        fname_str = fname_str.substr(0, 20) + "...::lambda";
    } else if (start && end)
        fname_str  = fname_str.substr(start-fname+1, end-start+1);

    if (color)
        fprintf(f, "%s[%s](%i) ", color, fname_str.c_str(), getpid());
    else
        fprintf(f, "[%s](%i) ", fname_str.c_str(), getpid());
    fflush(f);
}

void _log_buf(const std::vector<uint8_t> &buf, size_t max)
{
    if (logfd == nullptr)
        throw dint::Exception("log file is not open");

    const size_t n = buf.size();
    max = (max < n ? max : n);

    fprintf(logfd, "size %lu\n", n);
    if (!printbuf) {
        fflush(logfd);
        return;
    }

    size_t col = 0;
    for (size_t i = 0; i < max; ++i)
    {
        char a = buf[i];
        char as[2] = {a, 0};
        const char *str = as;

        switch (a)
        {
            case '\r':
                str = "\\r";
                break;
            case '\n':
                str = "\\n";
                break;
            case '\t':
                str = "\\t";
                break;
        }
        if (!(++col % 80) && i+1 != max)
            fprintf(logfd, "%s\n", str);
        else
            fprintf(logfd, "%s", str);
    }

    if (max < n) {
        fprintf(logfd, "...\n");
    } else {
        fprintf(logfd, "\n");
    }

    fflush(logfd);
}


void _log_error(const char *fmt...)
{
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "%s", tcolors::RED);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "%s", tcolors::RESET);

    fflush(stderr);
    va_end(args);
}

void _log(const char *fmt...)
{
    if (logfd == nullptr)
        throw dint::Exception("log file is not open");

    va_list args;
    va_start(args, fmt);
    vfprintf(logfd, fmt, args);
    fflush(logfd);
    va_end(args);
}

void _log_color(const char *fmt...)
{
    if (logfd == nullptr)
        throw dint::Exception("log file is not open");

    va_list args;
    va_start(args, fmt);

    vfprintf(logfd, fmt, args);
    fprintf(logfd, "%s", tcolors::RESET);

    fflush(logfd);
    va_end(args);
}

void _log_color_clear(const char *fmt...)
{
    if (logfd == nullptr)
        throw dint::Exception("log file is not open");

    va_list args;
    va_start(args, fmt);

    fprintf(logfd, "%s", tcolors::RESET);
    vfprintf(logfd, fmt, args);
    fprintf(logfd, "%s", tcolors::RESET);

    fflush(logfd);
    va_end(args);
}

void _log_color_set(const char *color, const char *fmt...)
{
    if (logfd == nullptr)
        throw dint::Exception("log file is not open");

    va_list args;
    va_start(args, fmt);

    fprintf(logfd, "%s", color);
    vfprintf(logfd, fmt, args);
    fprintf(logfd, "%s", tcolors::RESET);

    fflush(logfd);
    va_end(args);
}
