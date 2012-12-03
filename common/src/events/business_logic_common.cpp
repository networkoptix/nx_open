#include "business_logic_common.h"

#include <serializer.h>
#include <parser.h>


namespace ToggleState
{
    QString toString( Value val )
    {
        switch( val )
        {
            case Off:
                return QLatin1String("off");
            case On:
                return QLatin1String("on");
            case Any:
                return QLatin1String("any");
            case NotDefined:
                return QLatin1String("not defined");
            default:
                return QLatin1String("Unknown");
        }
    }
}

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
