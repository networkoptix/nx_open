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

/*
* @param systemName - new system name
* @param sysIdTime - database recovery time (last back time)
* @param tranLogTime - move transaction time to position at least tranLogTime
*/
bool changeSystemName(const QString &systemName, qint64 sysIdTime, qint64 tranLogTime);

#endif // _SERVER_UTIL_H
