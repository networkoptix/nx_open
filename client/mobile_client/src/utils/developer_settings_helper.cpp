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
    const auto& log = QnLog::instance();
    return QnLog::logLevelToString(log ? log->logLevel() : cl_logUNKNOWN);
}

void DeveloperSettingsHelper::setLogLevel(QString logLevel)
{
    if (auto& log = QnLog::instance())
    {
        log->setLogLevel(QnLog::logLevelFromString(logLevel));
        emit logLevelChanged();
    }
}

} // namespace utils
} // namespace mobile
} // namespace client
} // namespace nx
