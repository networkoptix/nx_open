#ifndef _SERVER_UTIL_H
#define _SERVER_UTIL_H

#include "core/resource/media_server_resource.h"

QString authKey();
QString serverGuid();
QString getDataDirectory();
QString defaultStoragePath();
void syncStoragesToSettings(QnMediaServerResourcePtr server);
void setMediaServerResource(QnMediaServerResourcePtr server);
QnMediaServerResourcePtr getMediaServerResource();

#endif // _SERVER_UTIL_H
