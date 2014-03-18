/**********************************************************
* 31 oct 2012
* a.kolesnikov
***********************************************************/

#include "networkoptixmodulerevealcommon.h"

#include <map>


namespace Serialization
{
    template<class T>
        bool deserializeSimpleType( T* const val, const quint8** const bufStart, const quint8* bufEnd )
    {
        if( bufEnd - *bufStart < (qint32)sizeof(*val) )
            return false;
        memcpy( val, *bufStart, sizeof(*val) );
        *bufStart += sizeof(*val);
        return true;
    }

    template<class T>
        bool serializeSimpleType( const T& val, quint8** const bufStart, const quint8* bufEnd )
    {
        if( bufEnd - *bufStart < (qint32)sizeof(val) )
            return false;
        memcpy( *bufStart, &val, sizeof(val) );
        *bufStart += sizeof(val);
        return true;
    }

    bool serialize( const quint64& val, quint8** const bufStart, const quint8* bufEnd )
    {
        return serializeSimpleType( val, bufStart, bufEnd );
    }

    bool deserialize( quint64* const val, const quint8** const bufStart, const quint8* bufEnd )
    {
        return deserializeSimpleType( val, bufStart, bufEnd );
    }

    bool serialize( const qint64& val, quint8** const bufStart, const quint8* bufEnd )
    {
        return serializeSimpleType( val, bufStart, bufEnd );
    }

    bool deserialize( qint64* const val, const quint8** const bufStart, const quint8* bufEnd )
    {
        return deserializeSimpleType( val, bufStart, bufEnd );
    }

    bool serialize( const quint16& val, quint8** const bufStart, const quint8* bufEnd )
    {
        return serializeSimpleType( val, bufStart, bufEnd );
    }

    bool deserialize( quint16* const val, const quint8** const bufStart, const quint8* bufEnd )
    {
        return deserializeSimpleType( val, bufStart, bufEnd );
    }

    bool serialize( const quint32& val, quint8** const bufStart, const quint8* bufEnd )
    {
        return serializeSimpleType( val, bufStart, bufEnd );
    }

    bool deserialize( quint32* const val, const quint8** const bufStart, const quint8* bufEnd )
    {
        return deserializeSimpleType( val, bufStart, bufEnd );
    }

    bool serialize( const QString& val, quint8** const bufStart, const quint8* bufEnd )
    {
        if( !serialize( (quint32)val.size(), bufStart, bufEnd ) )
            return false;
        if( bufEnd - *bufStart < val.size() )
            return false;
        memcpy( *bufStart, val.toLatin1().data(), val.size() );
        *bufStart += val.size();
        return true;
    }

    bool deserialize( QString* const val, const quint8** const bufStart, const quint8* bufEnd )
    {
        quint32 strLength = 0;
        if( !deserialize( &strLength, bufStart, bufEnd ) )
            return false;
        if( bufEnd - *bufStart < (qint32)strLength )
            return false;
        *val = QString::fromLatin1( reinterpret_cast<const char*>(*bufStart), strLength );
        *bufStart += strLength;
        return true;
    }
}


static const QString revealRequestStr = lit("{ magic: \"7B938F06-ACF1-45f0-8303-98AA8057739A\" }");

bool RevealRequest::serialize( quint8** const bufStart, const quint8* bufEnd )
{
    if( bufEnd - *bufStart < revealRequestStr.size() )
        return false;

    memcpy( *bufStart, revealRequestStr.toLatin1().data(), revealRequestStr.size() );
    *bufStart += revealRequestStr.size();
    return true;
}

bool RevealRequest::deserialize( const quint8** bufStart, const quint8* bufEnd )
{
    if( bufEnd - *bufStart < revealRequestStr.size() )
        return false;

    if( memcmp( *bufStart, revealRequestStr.toLatin1().data(), revealRequestStr.size() ) != 0 )
        return false;
    *bufStart += revealRequestStr.size();
    return true;
}

/*
{
    'version' : APPLICATION_VERSION,
    'port' : port,
    'application' : APPLICATION_NAME,
    'customization' : CUSTOMIZATION
}
*/

RevealResponse::RevealResponse()
{
}

bool RevealResponse::serialize( quint8** const bufStart, const quint8* bufEnd )
{
    QString str = lit("{\n'application': %1,\n'version': %2,\n'customization': %3,\n'seed': %4").arg(type).arg(version).arg(customization).arg(seed);
    for( TypeSpecificParamMap::const_iterator
        it = typeSpecificParameters.begin();
        it != typeSpecificParameters.end();
        ++it )
    {
        str += lit(",\n '%1': '%2'").arg(it.key()).arg(it.value());
    }
    str += lit("}");

    return Serialization::serialize( str, bufStart, bufEnd );
}

bool RevealResponse::deserialize( const quint8** bufStart, const quint8* bufEnd )
{
    enum State
    {
        sInit,
        sReadingMap,
        sReadingMapKey,
        waitingMapKeyValueSeparator,
        waitingMapValue,
        sReadingMapValue,
        waitingMapElementSeparator,
        finish
    }
    state = sInit;

    std::map<QString, QString> mapElements;

    QString currentToken;
    QString prevKeyName;
    bool currentTokenQuoted = false;
    for( const char* ch = (const char*)*bufStart; ch < (const char*)bufEnd; ++ch )
    {
        bool isSeparator = false;
        bool isPrintedChar = false;
        if( *ch == ' ' || *ch == '\t' || *ch == '\n' || *ch == '\r' )
        {
            isSeparator = true;
        }
        else if( *ch == '\'' || *ch == '"' || *ch == ',' )
        {
        }
        else
        {
            //printed char
            isPrintedChar = true;
        }

        switch( state )
        {
            case sInit:
                if( *ch == '{' )
                    state = sReadingMap;
                break;

            case sReadingMap:
                if( isPrintedChar )
                {
                    state = sReadingMapKey;
                    currentToken.clear();
                    currentToken += QString::fromLatin1(ch, 1);
                }
                break;

            case sReadingMapKey:
                if( isPrintedChar )
                    currentToken += QString::fromLatin1(ch, 1);
                else if( isSeparator || *ch == '"' || *ch == '\'' )
                {
                    state = waitingMapKeyValueSeparator;
                    prevKeyName = currentToken;
                }
                break;

            case waitingMapKeyValueSeparator:
                if( *ch == ':' )
                    state = waitingMapValue;
                break;

            case waitingMapValue:
                if( isPrintedChar )
                {
                    currentToken.clear();
                    state = sReadingMapValue;
                    currentToken += QString::fromLatin1(ch, 1);
                    currentTokenQuoted = false;
                }
                else if( *ch == '\'' || *ch == '"' )
                {
                    currentToken.clear();
                    state = sReadingMapValue;
                    currentTokenQuoted = true;
                }
                break;

            case sReadingMapValue:
                if( isPrintedChar || (isSeparator && currentTokenQuoted) )
                {
                    currentToken += QString::fromLatin1(ch, 1);
                    break;
                }
                else if( isSeparator || *ch == '"' || *ch == '\'' )
                    state = waitingMapElementSeparator;
                else if( *ch == ',' )
                    state = sReadingMap;
                else if( *ch == '}' )
                    state = finish;
                else
                    break;
                mapElements.insert( std::make_pair( prevKeyName, currentToken ) );
                currentToken.clear();
                break;

            case waitingMapElementSeparator:
                if( *ch == ',' )
                    state = sReadingMap;
                else if( *ch == '}' )
                    state = finish;
                break;

            case finish:
                break;
        }

        if( state == finish )
            break;
    }

    for( std::map<QString, QString>::const_iterator
        it = mapElements.begin();
        it != mapElements.end();
        ++it )
    {
        if( it->first == lit("application") )
            type = it->second;
        else if( it->first == lit("version") )
            version = it->second;
        else if( it->first == lit("customization") )
            customization = it->second;
        else if( it->first == lit("seed") )
            seed = it->second;
        else
            typeSpecificParameters.insert( it->first, it->second );
    }

    return !type.isEmpty() && !version.isEmpty();
}
