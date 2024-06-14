#pragma once

#include <fmt/core.h>

namespace rana {
namespace log {

template <typename... T>
void info(fmt::format_string<T...> fmt, T&&... args)
{
    fmt::println(stderr, fmt, std::forward<T>(args)...);
}

template <typename... T>
void warn(fmt::format_string<T...> fmt, T&&... args)
{
    fmt::println(stderr, "warn: {}", fmt::format(fmt, std::forward<T>(args)...));
}

template <typename... T>
void err(fmt::format_string<T...> fmt, T&&... args)
{
    fmt::println(stderr, "err: {}", fmt::format(fmt, std::forward<T>(args)...));
}

}
}
