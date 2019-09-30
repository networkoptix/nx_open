#include <nx/utils/log/log_initializer.h>
#include <nx/utils/log/log_main.h>
#include <nx/utils/log/log_settings.h>

namespace nx::vms::server::test::test_support {

void createTestLogger(const std::set<utils::log::Filter>& logFilters, nx::utils::log::Buffer** outBuffer)
{
    namespace log = nx::utils::log;
    log::unlockConfiguration();

    log::mainLogger()->setDefaultLevel(log::Level::none);

    log::Settings settings;
    settings.loggers.resize(1);
    settings.loggers.front().level.primary = log::levelFromString("VERBOSE");

    auto logWriter = std::unique_ptr<log::AbstractWriter>(*outBuffer = new log::Buffer);
    addLogger(buildLogger(settings, QString("log_ut"), QString(), logFilters, std::move(logWriter)));
}

} // namespace nx::vms::server::test::test_support
