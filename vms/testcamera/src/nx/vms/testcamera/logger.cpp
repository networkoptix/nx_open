#include "logger.h"

#include <iostream>

#include <nx/vms/testcamera/test_camera_ini.h>
#include <nx/utils/log/log.h>
#include <nx/kit/utils.h>
#include <nx/utils/switch.h>

namespace nx::vms::testcamera {

QString us(std::chrono::microseconds value)
{
    return lm("%1 us").args(value.count());
}

QString us(std::optional<std::chrono::microseconds> value)
{
    if (!NX_ASSERT(value))
        return "UNKNOWN us";

    return us(*value);
}

QString us(int64_t valueUs)
{
    return us(std::chrono::microseconds(valueUs));
}

nx::utils::SharedGuard Logger::pushContext(const QString& contextCaption)
{
    pushContextCaption(contextCaption);

    return nx::utils::SharedGuard([this, contextCaption]() { popContextCaption(contextCaption); });
}

std::unique_ptr<Logger> Logger::cloneForContext(const QString& contextCaption) const
{
    auto newLogger = std::make_unique<Logger>(m_tag);
    newLogger->m_contextCaptions = m_contextCaptions;
    newLogger->m_context = m_context;
    newLogger->pushContextCaption(contextCaption);
    return newLogger;
}

void Logger::pushContextCaption(const QString& contextCaption)
{
    m_contextCaptions.push_back(contextCaption);
    updateContext();
}

void Logger::popContextCaption(const QString& contextCaption)
{
    static constexpr char errorMessage[] = "Logger: Unable to restore previous context";

    if (!NX_ASSERT(!m_contextCaptions.empty(), lm("%1: it is missing.").args(errorMessage)))
        return;

    if (!NX_ASSERT(m_contextCaptions.back() == contextCaption,
        lm("%1: expected %2, attempted %3.").args(
            errorMessage,
            nx::kit::utils::toString(m_contextCaptions.back()),
            nx::kit::utils::toString(contextCaption))))
    {
        return;
    }

    m_contextCaptions.pop_back();
    updateContext();
}

/** Rebuilds m_context. */
void Logger::updateContext()
{
    m_context = "";
    for (const QString& context: m_contextCaptions)
    {
        if (!m_context.isEmpty())
            m_context.append(", ");
        m_context.append(context);
    }
    if (!m_context.isEmpty())
        m_context.prepend(" ");
}

} // namespace nx::vms::testcamera
