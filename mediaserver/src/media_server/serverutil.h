#ifndef _SERVER_UTIL_H
#define _SERVER_UTIL_H

#include <core/resource/resource_fwd.h>

// TODO: #Elric this belongs together with server_settings

QString authKey();
QnUuid serverGuid();
void setUseAlternativeGuid(bool value);

QString getDataDirectory();
void syncStoragesToSettings(const QnMediaServerResourcePtr &server);
bool backupDatabase();

#endif // _SERVER_UTIL_H
