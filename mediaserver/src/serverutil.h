#ifndef _SERVER_UTIL_H
#define _SERVER_UTIL_H

#include "core/resource/media_server_resource.h"

QString serverGuid();
QString getDataDirectory();
QString defaultStoragePath();
void syncStoragesToSettings(QnMediaServerResourcePtr server);

#endif // _SERVER_UTIL_H
