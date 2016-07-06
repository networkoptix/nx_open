#pragma once

#include <nx/utils/argument_parser.h>

namespace nx {
namespace utils {

class TestOptions
{
public:
    static void setTimeoutMultiplier(size_t value);
    static size_t timeoutMultiplier();
    static void disableTimeAsserts(bool areDisabled = true);
    static bool areTimeAssertsDisabled();

    static void applyArguments(const ArgumentParser& args);

private:
    static std::atomic<size_t> s_timeoutMultiplier;
    static std::atomic<bool> s_disableTimeAsserts;
};

} // namespace utils
} // namespace nx
