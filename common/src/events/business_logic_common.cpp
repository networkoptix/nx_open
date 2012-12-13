#include "business_logic_common.h"

/** This modules are part of QJson package */
#include <serializer.h>
#include <parser.h>


namespace ToggleState
{
    const char* toString( Value val )
    {
        switch( val )
        {
            case Off:
                return "off";
            case On:
                return "on";
            case Any:
                return "any";
            case NotDefined:
                return "not defined";
            default:
                return "Unknown";
        }
    }

    Value fromString( const char* str )
    {
        if( strcmp( str, "off" ) == 0 )
            return Off;
        else if( strcmp( str, "on" ) == 0 )
            return On;
        else if( strcmp( str, "any" ) == 0 )
            return Any;
        else
            return NotDefined;
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
