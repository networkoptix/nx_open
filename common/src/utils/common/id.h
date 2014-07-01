#ifndef QN_ID_H
#define QN_ID_H

#include <QtCore/QString>
#include <QtCore/QMetaType>
#include <QtCore/QUuid>
#include <QCryptographicHash>

#include <utils/network/socket.h>
#include <common/common_globals.h>

typedef QUuid QnId; // TODO: #Elric remove this typedef. It's useless and it prevents forward declarations.


inline QnId intToGuid(qint32 value, const QByteArray& postfix)
{
    QCryptographicHash md5Hash( QCryptographicHash::Md5 );
    value = htonl(value);
    md5Hash.addData((const char*) &value, sizeof(value));
    md5Hash.addData(postfix);
    QByteArray ha2 = md5Hash.result();
    return QnId::fromRfc4122(ha2);
}

inline QString guidToSqlString(const QnId& guid)
{
    QByteArray data = guid.toRfc4122().toHex();
    return QString(lit("x'%1'")).arg(QString::fromLatin1(data));
}



#endif // QN_ID_H
