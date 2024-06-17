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
    std::fputs("warn: ", stderr);
    info(fmt, std::forward<T>(args)...);
}

template <typename... T>
void err(const char *fmt, T&&... args)
{
    std::fputs("err: ", stderr);
    info(fmt, std::forward<T>(args)...);
}

}
}
