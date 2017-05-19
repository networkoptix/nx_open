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
    nx::utils::log::mainLogger()->setDefaultLevel(nx::utils::log::levelFromString(logLevel));
}

} // namespace utils
} // namespace mobile
} // namespace client
} // namespace nx
