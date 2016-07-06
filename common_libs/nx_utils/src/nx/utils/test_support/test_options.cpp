#include "test_options.h"

#include <QDebug>

namespace nx {
namespace utils {

void TestOptions::setTimeoutMultiplier(size_t value)
{
    s_timeoutMultiplier = value;
    qWarning() << ">>> TestOptions::setTimeoutMultiplier(" << value << ") <<<";
}

size_t TestOptions::timeoutMultiplier()
{
    return s_timeoutMultiplier;
}

void TestOptions::disableTimeAsserts(bool areDisabled)
{
    s_disableTimeAsserts = areDisabled;
    qWarning() << ">>> TestOptions::disableTimeAsserts(" << areDisabled << ") <<<";
}

bool TestOptions::areTimeAssertsDisabled()
{
    return s_disableTimeAsserts;
}

void TestOptions::applyArguments(const utils::ArgumentParser& arguments)
{
    if (const auto value = arguments.get<size_t>(QLatin1String("timeout-multiplier")))
        setTimeoutMultiplier(*value);

    if (arguments.get(QLatin1String("disable-time-asserts")))
        disableTimeAsserts();
}

std::atomic<size_t> TestOptions::s_timeoutMultiplier(1);
std::atomic<bool> TestOptions::s_disableTimeAsserts(false);

} // namespace utils
} // namespace nx
