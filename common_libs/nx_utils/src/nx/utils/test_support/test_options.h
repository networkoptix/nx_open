#pragma once

#include <atomic>

#include <nx/utils/argument_parser.h>

namespace nx {
namespace utils {

class NX_UTILS_API TestOptions
{
public:
    static void setTimeoutMultiplier(size_t value);
    static size_t timeoutMultiplier();

    static void disableTimeAsserts(bool areDisabled = true);
    static bool areTimeAssertsDisabled();

    enum class LoadMode { light, normal, stress };
    static void setLoadMode(const QString& mode);
    static LoadMode getLoadMode();

    template<typename Count>
    static Count applyLoadMode(Count count);

    static void applyArguments(const ArgumentParser& args);

private:
    static std::atomic<size_t> s_timeoutMultiplier;
    static std::atomic<bool> s_disableTimeAsserts;
    static std::atomic<LoadMode> s_loadMode;
};

template<typename Count>
Count TestOptions::applyLoadMode(Count count)
{
    switch (s_loadMode.load())
    {
        case LoadMode::light: return 1;
        case LoadMode::normal: return count;
        case LoadMode::stress: return count * 100;
    }

    return count;
}

} // namespace utils
} // namespace nx
