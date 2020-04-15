#pragma once

#include <nx/utils/log/log_message.h>

std::string toString(std::chrono::system_clock::time_point time);

namespace detail {

void reportString(const QString& message);

} // namespace detail

/**
 * Prints the message to stdout, intended to be parsed by programs calling this tool.
 *
 * ATTENTION: The format of such messages should be changed responsibly, revising their usages.
 */
template<typename Format, typename... Args>
void report(const Format& format, const Args&... args)
{
    detail::reportString(nx::format(format, args...));
}
