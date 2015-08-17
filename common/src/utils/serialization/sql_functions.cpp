#include "sql_functions.h"
#include "api/model/audit/auth_session.h"
#include "utils/network/http/qnbytearrayref.h"

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
    serialize_field(authData.toByteArray(), target);
}

void deserialize_field(const QVariant &value, QnAuthSession *target)
{
    QByteArray tmp;
    deserialize_field(value, &tmp);
    target->fromByteArray(tmp);
}
