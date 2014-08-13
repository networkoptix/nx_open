#include "common_globals.h"


namespace Qn
{
    const char* serializationFormatToHttpContentType(SerializationFormat format)
    {
        switch( format )
        {
            case JsonFormat:
                return "application/json";
            case UbjsonFormat:
                return "application/ubjson";
            case BnsFormat:
                return "application/octet-stream";
            case CsvFormat:
                return "text/csv";
            case XmlFormat:
                return "application/xml";
            default:
                assert(false);
                return "unsupported";
        }
    }

    SerializationFormat serializationFormatFromHttpContentType(const QByteArray& httpContentType)
    {
        if( httpContentType == "application/json" )
            return JsonFormat;
        else if( httpContentType == "application/ubjson" )
            return UbjsonFormat;
        else if( httpContentType == "application/octet-stream" )
            return BnsFormat;
        else if( httpContentType == "text/csv" )
            return CsvFormat;
        else if( httpContentType == "application/xml" )
            return XmlFormat;
        else
        {
            assert(false);
            return UnsupportedFormat;
        }
    }
}