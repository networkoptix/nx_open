#include "business_logic_common.h"

/** This modules are part of QJson package */
#include <serializer.h>
#include <parser.h>


namespace ToggleState
{
    QString toString( Value val )
    {
        switch( val )
        {
            case Off:
                return QObject::tr("Stops");
            case On:
                return QObject::tr("Starts");
            case Any:
                return QObject::tr("Starts/stops");
            case NotDefined:
                return QObject::tr("Occurs");
        }
        return QLatin1String("---");
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
