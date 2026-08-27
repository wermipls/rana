#pragma once

#include <tl/expected.hpp>
#include <string>
#include <exception>
#include <source_location>

namespace rana {

class Exception : public std::exception {
    std::string msg;
    std::source_location location;

public:
    Exception(const std::string message,
              const std::source_location loc =
                    std::source_location::current())
             : location{loc}
    {
        msg = /* err("in ") + loc.function_name() + "\n" + */ 
              std::string(loc.file_name()) + ":"
            + std::to_string(loc.line()) + ": "
            + message;
    }

    virtual ~Exception() noexcept override = default;
    virtual const char *what() const noexcept override { return msg.c_str(); }
};

}
