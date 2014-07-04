#ifndef QN_ID_H
#define QN_ID_H

#include <QtCore/QString>
#include <QtCore/QMetaType>
#include <QtCore/QUuid>

#include <utils/network/socket.h>
#include <common/common_globals.h>

typedef QUuid QnId; // TODO: #Elric remove this typedef. It's useless and it prevents forward declarations.


inline QnId intToGuid(qint32 value)
{
    QByteArray data(16, 0);
    *((quint32*) data.data()) = htonl(value);
    return QnId::fromRfc4122(data);
}

inline int guidToInt(const QnId& guid)
{
    QByteArray data = guid.toRfc4122();
    return ntohl(*((quint32*) data.data()));
}

inline QString guidToSqlString(const QnId& guid)
{
    QByteArray data = guid.toRfc4122().toHex();
    return QString(lit("x'%1'")).arg(QString::fromLatin1(data));
}

typedef QSet<QnId> QnIdSet;

#endif // QN_ID_H
