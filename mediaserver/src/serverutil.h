#ifndef _SERVER_UTIL_H
#define _SERVER_UTIL_H

#include "core/resource/video_server_resource.h"

QString serverGuid();
QString getDataDirectory();
QString defaultStoragePath();
void syncStoragesToSettings(QnVideoServerResourcePtr server);

#endif // _SERVER_UTIL_H
