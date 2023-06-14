#ifndef LOG_DEBUG_ENABLE
#define LOG_DEBUG_ENABLE
#include <iostream>
#include <string>

#include "../Logger.h"
#include "../Timestamp.h"

int main() {
    std::string author = "Ephmeral";
    LOG_DEBUG("this is a log debug mesasage, wirte by %s", author.c_str());
    LOG_INFO("this is a log info message");
    LOG_ERROR("this is a log error message");
    LOG_FATAL("this is a log fatal message");
    return 0;
}
#endif