// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "developer_settings_helper.h"

#include <mobile_client/mobile_client_settings.h>
#include <nx/utils/log/log.h>

namespace nx {
namespace client {
namespace mobile {
namespace utils {

DeveloperSettingsHelper::DeveloperSettingsHelper(QObject* parent):
    QObject(parent)
{
    setLogLevel(qnSettings->logLevel());
}

QString DeveloperSettingsHelper::logLevel() const
{
    return nx::log::toString(nx::log::mainLogger()->defaultLevel());
}

void DeveloperSettingsHelper::setLogLevel(QString logLevel)
{
    const auto level = nx::log::levelFromString(logLevel);
    if (level == nx::log::mainLogger()->defaultLevel())
        return;

    nx::log::mainLogger()->setDefaultLevel(level);
    qnSettings->setLogLevel(nx::log::toString(level));
    qnSettings->save();

    emit logLevelChanged();
}

} // namespace utils
} // namespace mobile
} // namespace client
} // namespace nx
