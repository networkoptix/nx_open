#include <QSettings>
#include "version.h"

#if defined(Q_OS_LINUX)
QString configFileName = QString("/opt/%1/mediaserver/etc/mediaserver.conf").arg(VER_LINUX_ORGANIZATION_NAME);
QSettings qSettings(configFileName, QSettings::IniFormat);
#else
QSettings qSettings(QSettings::SystemScope, QN_ORGANIZATION_NAME, QN_APPLICATION_NAME);
#endif
