#include "sql_functions.h"

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
    NX_ASSERT(tmp.size() % 16 == 0);
    const char* data = tmp.data();
    const char* dataEnd = data + tmp.size();
    for(; data < dataEnd; data += 16)
        target->push_back(QnUuid::fromRfc4122(QByteArray::fromRawData(data, 16)));
}
