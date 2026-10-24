// errorHandling/asserts.hpp

#pragma once

#include <cstdlib>
#include <iostream>
#include <source_location>
#include <sstream>

template <typename... MessageParts>
inline void AssertFail(const char* condition, std::source_location location,
                       const MessageParts&... message_parts)
{
    std::ostringstream message;

    (message << ... << message_parts);

    std::cerr << "Assertion failed\n"
              << "Condition: " << condition << '\n'
              << "Message: " << message.str() << '\n'
              << "File: " << location.file_name() << '\n'
              << "Line: " << location.line() << '\n'
              << "Function: " << location.function_name() << '\n';

    std::abort();
}

#define ASSERT(condition, ...)                                                                     \
    do                                                                                             \
    {                                                                                              \
        if (!(condition))                                                                          \
        {                                                                                          \
            AssertFail(#condition, std::source_location::current(), __VA_ARGS__);                  \
        }                                                                                          \
    } while (false)
