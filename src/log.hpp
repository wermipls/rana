#pragma once

#include <cstdio>
#include <utility>

namespace rana {
namespace log {

// those warnings are pretty much all false positives and largely useless...
// using a macro would yield better debugging info 🙄 the wonders of c++
#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wformat-security"
#endif

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

#ifdef __clang__
    #pragma clang diagnostic pop
#endif

}
}
