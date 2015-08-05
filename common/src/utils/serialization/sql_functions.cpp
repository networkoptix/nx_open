#include "sql_functions.h"
#include "api/model/audit/auth_session.h"
#include "utils/network/http/qnbytearrayref.h"

namespace {
    char DELIMITER = '$';
}

void serialize_field(const std::vector<QnUuid>&value, QVariant *target) 
{
    QByteArray result;
    for (const auto& id: value)
        result.append(id.toRfc4122());
    serialize_field(result, target);
}

void deserialize_field(const QVariant &value, std::vector<QnUuid> *target)
{
    QByteArray tmp;
    deserialize_field(value, &tmp);
    Q_ASSERT(tmp.size() % 16 == 0);
    const char* data = tmp.data();
    const char* dataEnd = data + tmp.size();
    for(; data < dataEnd; data += 16)
        target->push_back(QnUuid::fromRfc4122(QByteArray::fromRawData(data, 16)));
}

void serialize_field(const QnAuthSession&authData, QVariant *target) 
{
    // todo: need to add encode/decode in a future version. DELIMITER is reserved symbol so far
    QByteArray result;
    result.append(authData.id.toByteArray());
    result.append(DELIMITER);
    result.append(authData.userName.toUtf8());
    result.append(DELIMITER);
    result.append(authData.userHost.toUtf8());
    result.append(DELIMITER);
    result.append(authData.userAgent.toUtf8());
    serialize_field(result, target);
}

void deserialize_field(const QVariant &value, QnAuthSession *target)
{
    QByteArray tmp;
    deserialize_field(value, &tmp);
    QnByteArrayConstRef ref(tmp);
    QList<QnByteArrayConstRef> params = ref.split(DELIMITER);
    if (params.size() > 0)
        target->id = QnUuid::fromRfc4122(params[0]);
    if (params.size() > 1)
        target->userName = QString::fromUtf8(params[1]);
    if (params.size() > 2)
        target->userHost = QString::fromUtf8(params[2]);
    if (params.size() > 3)
        target->userAgent = QString::fromUtf8(params[3]);
}
