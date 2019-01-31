#ifndef QN_ID_H
#define QN_ID_H

#include <string>

#include <QCryptographicHash>
#include <QtCore/QString>
#include <QtCore/QMetaType>
#include <nx/utils/uuid.h>
#include <QtCore/QtEndian>
#include <QCryptographicHash>

#include <nx/utils/literal.h>

#include <common/common_globals.h>

inline QnUuid intToGuid(qint32 value, const QByteArray& postfix)
{
    QCryptographicHash md5Hash( QCryptographicHash::Md5 );
    value = qToBigEndian(value);
    md5Hash.addData((const char*) &value, sizeof(value));
    md5Hash.addData(postfix);
    QByteArray ha2 = md5Hash.result();
    return QnUuid::fromRfc4122(ha2);
}

inline QString guidToSqlString(const QnUuid& guid)
{
    QByteArray data = guid.toRfc4122().toHex();
    return QString(lit("x'%1'")).arg(QString::fromLatin1(data));
}


inline QnUuid guidFromArbitraryData(const QByteArray& data)
{
    return QnUuid::fromArbitraryData(data);
}

inline QnUuid guidFromArbitraryData(const QString& data)
{
    return QnUuid::fromArbitraryData(data);
}

inline QnUuid guidFromArbitraryData(const std::string& data)
{
    return QnUuid::fromArbitraryData(data);
}

#endif // QN_ID_H
