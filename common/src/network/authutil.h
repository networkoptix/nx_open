#ifndef NETWORK_AUTHUTIL_H_
#define NETWORK_AUTHUTIL_H_

#include <QtCore/QMap>
#include <QtCore/QByteArray>
#include <QStringList>

QList<QByteArray> smartSplit(const QByteArray& data, const char delimiter);
QStringList smartSplit(const QString& data, const QChar delimiter);
QByteArray unquoteStr(const QByteArray& v);
QMap<QByteArray, QByteArray> parseAuthData(const QByteArray &authData, char delimiter);

#endif