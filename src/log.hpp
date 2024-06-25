#pragma once

#include <cstdio>

namespace rana {
namespace log {

template <typename... T>
void info(const char *fmt, T&&... args)
{
    std::fprintf(stderr, fmt, std::forward<T>(args)...);
    std::fputc('\n', stderr);
}

template <typename... T>
void warn(const char *fmt, T&&... args)
{
    std::fputs("\033[35mwarn: \033[0m", stderr);
    info(fmt, std::forward<T>(args)...);
}

template <typename... T>
void err(const char *fmt, T&&... args)
{
    std::fputs("\033[31merr: \033[0m", stderr);
    info(fmt, std::forward<T>(args)...);
}

}
}
