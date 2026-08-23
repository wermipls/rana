#pragma once

#include <cstdio>
#include <utility>
#ifndef NDEBUG
    #include <source_location>
    #if _WIN32
        #include <debugapi.h>
    #endif
#endif

namespace rana {
namespace log {

// those warnings are pretty much all false positives and largely useless...
// using a macro would yield better debugging info 🙄 the wonders of c++
#ifdef __clang__
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wformat-security"
#endif

#ifndef NDEBUG
    template <typename... T>
    struct debug {
        debug(
            const char *fmt,
            T&&... args,
            const std::source_location &loc = std::source_location::current())
        {
            char msg[1024];
            auto prefix = snprintf(msg, sizeof(msg), "%s:%d: ", loc.file_name(), loc.line());
            std::snprintf(msg+prefix, sizeof(msg)-prefix, fmt, std::forward<T>(args)...);
            OutputDebugStringA(msg);
            std::fputs("\033[35mdebug: \033[0m", stderr);
            std::fputs(msg, stderr);
            std::fputc('\n', stderr);
        }
    };
    template <typename... T>
    debug(const char *fmt, T&&...args) -> debug<T...>;
#else
    // no-op.
    template <typename... T> void debug(T&&...) {};
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
