#include "business_logic_common.h"

#include <qjson/serializer.h>
#include <qjson/parser.h>

QByteArray serializeBusinessParams(const QnBusinessParams& params)
{
    QJson::Serializer serializer;

    QVariantMap paramMap;
    foreach(QString key, params.keys())
    {
        paramMap[key] = params[key];
    }

    return serializer.serialize(paramMap);
}

QnBusinessParams deserializeBusinessParams(const QByteArray& value)
{
    QJson::Parser parser;
    bool ok;

    QTextStream io(value);
    QVariant result = parser.parse(io.device(), &ok);
    if (ok)
        return result.toMap();

    return QnBusinessParams();
}
