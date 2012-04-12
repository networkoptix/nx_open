#include <QSettings>
#include "version.h"

#if defined(Q_OS_LINUX)
QSettings qSettings("/opt/networkoptix/mediaserver/etc/mediaserver.conf", QSettings::IniFormat);
#else
QSettings qSettings(QSettings::SystemScope, ORGANIZATION_NAME, APPLICATION_NAME);
#endif
