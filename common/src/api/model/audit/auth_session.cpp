#include "auth_session.h"
#include <nx/network/http/qnbytearrayref.h>

namespace {
    char DELIMITER = '$';
}


QByteArray QnAuthSession::toByteArray() const
{
    auto encoded = [](QByteArray value) { 
        return value.replace(DELIMITER, char('_')); 
    };

    QByteArray result;
    result.append(id.toByteArray());
    result.append(DELIMITER);
    result.append(encoded(userName.toUtf8()));
    result.append(DELIMITER);
    result.append(encoded(userHost.toUtf8()));
    result.append(DELIMITER);
    result.append(encoded(userAgent.toUtf8()));
    
    return result;
}

void QnAuthSession::fromByteArray(const QByteArray& data)
{
    QnByteArrayConstRef ref(data);
    QList<QnByteArrayConstRef> params = ref.split(DELIMITER);
    if (params.size() > 0)
        id = QnUuid(params[0]);
    if (params.size() > 1)
        userName = QString::fromUtf8(params[1]);
    if (params.size() > 2)
        userHost = QString::fromUtf8(params[2]);
    if (params.size() > 3)
        userAgent = QString::fromUtf8(params[3]);
}
