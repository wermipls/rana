#pragma once

#include <format>
#include <cstdio>

namespace rana {
namespace log {

template <typename... T>
void info(std::format_string<T...> fmt, T&&... args)
{
    std::fputs(std::format(fmt, std::forward<T>(args)...).c_str(), stderr);
    std::fputs("\n", stderr);
}

template <typename... T>
void warn(std::format_string<T...> fmt, T&&... args)
{
    std::fputs("warn: ", stderr);
    info(fmt, std::forward<T>(args)...);
}

template <typename... T>
void err(std::format_string<T...> fmt, T&&... args)
{
    std::fputs("err: ", stderr);
    info(fmt, std::forward<T>(args)...);
}

}
}
