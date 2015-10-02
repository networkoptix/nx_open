#ifndef NETWORK_AUTHUTIL_H_
#define NETWORK_AUTHUTIL_H_

#include <QtCore/QMap>
#include <QtCore/QByteArray>
#include <QStringList>

//!Splits \a data by \a delimiter not closed within quotes
/*!
    E.g.: 
    \code
    one, two, "three, four"
    \endcode

    will be splitted to 
    \code
    one
    two
    "three, four"
    \endcode
*/
QList<QByteArray> smartSplit(const QByteArray& data, const char delimiter);
QStringList smartSplit(const QString& data, const QChar delimiter);

QByteArray unquoteStr(const QByteArray& v);
QString unquoteStr(const QString& v);
QMap<QByteArray, QByteArray> parseAuthData(const QByteArray &authData, char delimiter);

#endif