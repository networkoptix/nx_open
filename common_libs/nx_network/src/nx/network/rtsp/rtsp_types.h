/**********************************************************
* 31 jul 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_RTSP_TYPES_H
#define NX_RTSP_TYPES_H

#include "../http/http_types.h"


namespace nx_rtsp
{
    NX_NETWORK_API extern const char* const kUrlSchemeName;
    NX_NETWORK_API extern const char* const kSecureUrlSchemeName;

    const int DEFAULT_RTSP_PORT = 554;

    static const nx_http::MimeProtoVersion rtsp_1_0 = { "RTSP", "1.0" };

    namespace header
    {
        class NX_NETWORK_API Range
        {
        public:
            static const nx_http::StringType NAME;
        };
    }

    class NX_NETWORK_API RtspResponse : public nx_http::Response
    {
    public:
        bool parse( const nx_http::ConstBufferRefType& data);
    };

    //!Parses Range header ([rfc2326, 12.29])
    /*!
        Returns range in microseconds
        \note only \a clock is supported. Though, it MUST contain UTC timestamp (millis or usec). I.e., not rfc2326-compliant
        \note for \a now constant \a DATETIME_NOW is returned
        \return \a true if \a startTime and \a endTime were filled with values
    */
    bool NX_NETWORK_API parseRangeHeader( const nx_http::StringType& rangeStr, qint64* startTime, qint64* endTime );
}

#endif  //NX_RTSP_TYPES_H
