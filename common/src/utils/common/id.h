#ifndef QN_ID_H
#define QN_ID_H

#include <QtCore/QString>
#include <QtCore/QMetaType>
#include <QtCore/QUuid>
#include <QtCore/QtEndian>
#include <QCryptographicHash>

#include <common/common_globals.h>

typedef QUuid QnId; // TODO: #Elric remove this typedef. It's useless and it prevents forward declarations.
typedef QSet<QnId> QnIdSet;

inline QnId intToGuid(qint32 value, const QByteArray& postfix)
{
    QCryptographicHash md5Hash( QCryptographicHash::Md5 );
    value = qToBigEndian(value);
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

/** 
 * Create fixed QUuid from any data. As a value of uuid the MD5 hash is taken so created uuids 
 * will be equal for equal strings given. Also there is a collision possibility. 
 */ 
inline QnId guidFromArbitraryData(const QByteArray &data) {
    QCryptographicHash md5Hash( QCryptographicHash::Md5 );
    md5Hash.addData(data);
    QByteArray bytes = md5Hash.result();
    return QnId::fromRfc4122(bytes);
}

#endif // QN_ID_H
