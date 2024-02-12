// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "id.h"

#include <QtCore/QCryptographicHash>
#include <QtCore/QtEndian>

nx::Uuid intToGuid(qint32 value, const QByteArray& postfix)
{
    QCryptographicHash md5Hash( QCryptographicHash::Md5 );
    value = qToBigEndian(value);
    md5Hash.addData((const char*) &value, sizeof(value));
    md5Hash.addData(postfix);
    QByteArray ha2 = md5Hash.result();
    return nx::Uuid::fromRfc4122(ha2);
}

QString guidToSqlString(const nx::Uuid& guid)
{
    QByteArray data = guid.toRfc4122().toHex();
    return QString("x'%1'").arg(QString::fromLatin1(data));
}
