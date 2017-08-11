#include "developer_settings_helper.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace client {
namespace mobile {
namespace utils {

DeveloperSettingsHelper::DeveloperSettingsHelper(QObject* parent):
    QObject(parent)
{
}

QString DeveloperSettingsHelper::logLevel() const
{
    return nx::utils::log::toString(nx::utils::log::mainLogger()->defaultLevel());
}

void DeveloperSettingsHelper::setLogLevel(QString logLevel)
{
    const auto level = nx::utils::log::levelFromString(logLevel);
    if (level == nx::utils::log::mainLogger()->defaultLevel())
        return;

    nx::utils::log::mainLogger()->setDefaultLevel(level);
    emit logLevelChanged();
}

} // namespace utils
} // namespace mobile
} // namespace client
} // namespace nx
