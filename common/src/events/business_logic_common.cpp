#include "business_logic_common.h"

#include <serializer.h>
#include <parser.h>

QByteArray serializeBusinessParams(const QnBusinessParams& params)
{
    QJson::Serializer serializer;
    return serializer.serialize(QVariant(params));
}

QnBusinessParams deserializeBusinessParams(const QByteArray& value)
{
    QJson::Parser parser;
    bool ok;
    QVariant result = parser.parse(value, &ok);
    if (ok)
        return result.toMap();

    return QnBusinessParams();
}
