#ifndef _SERVER_UTIL_H
#define _SERVER_UTIL_H

#include <core/resource/resource_fwd.h>

// TODO: #Elric this belongs together with server_settings

QString authKey();
QUuid serverGuid();

QString getDataDirectory();
void syncStoragesToSettings(const QnMediaServerResourcePtr &server);

#endif // _SERVER_UTIL_H
