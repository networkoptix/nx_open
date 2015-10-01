#ifndef NETWORK_AUTHUTIL_H_
#define NETWORK_AUTHUTIL_H_

#include <QtCore/QMap>
#include <QtCore/QByteArray>

QList<QByteArray> smartSplit(const QByteArray& data, const char delimiter);

QByteArray unquoteStr(const QByteArray& v);

QMap<QByteArray, QByteArray> parseAuthData(const QByteArray &authData, char delimiter);

class HttpAuthenticationClientContext;

//!Calculates HA1 (see rfc 2617)
/*!
	\warning \a realm is not used currently
*/
 QByteArray createUserPasswordDigest(
    const QString& userName,
    const QString& password,
    const QString& realm );

 QByteArray createHttpQueryAuthParam(
     const QString& userName,
     const QString& password,
     const QString& realm,
     const QByteArray& method,
     QByteArray nonce);

#endif
