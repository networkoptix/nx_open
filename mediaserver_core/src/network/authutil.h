#ifndef NETWORK_AUTHUTIL_H_
#define NETWORK_AUTHUTIL_H_

#include <QtCore/QMap>
#include <QtCore/QByteArray>

QList<QByteArray> smartSplit(const QByteArray& data, const char delimiter);
QByteArray unquoteStr(const QByteArray& v);
QMap<QByteArray, QByteArray> parseAuthData(const QByteArray &authData, char delimiter);

#endif