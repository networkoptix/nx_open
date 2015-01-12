#ifndef _SERVER_UTIL_H
#define _SERVER_UTIL_H

#include <core/resource/resource_fwd.h>

// TODO: #Elric this belongs together with server_settings

QByteArray decodeAuthKey(const QByteArray&);
QnUuid serverGuid();
void setUseAlternativeGuid(bool value);

QString getDataDirectory();
void syncStoragesToSettings(const QnMediaServerResourcePtr &server);
bool backupDatabase();

bool isLocalAppServer(const QString &host);
bool changeSystemName(const QString &systemName);

#endif // _SERVER_UTIL_H
