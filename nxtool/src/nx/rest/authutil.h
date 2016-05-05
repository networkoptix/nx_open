#pragma once

#include <QtCore/QMap>
#include <QtCore/QByteArray>

namespace nx {
namespace rest {

QList<QByteArray> smartSplit(const QByteArray& data, const char delimiter);
QByteArray unquoteStr(const QByteArray& v);
QMap<QByteArray, QByteArray> parseAuthData(const QByteArray &authData, char delimiter);

} // namespace rest
} // namespace nx
