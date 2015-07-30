/**********************************************************
* 31 jul 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_RTSP_TYPES_H
#define NX_RTSP_TYPES_H

#include "../http/httptypes.h"


namespace nx_rtsp
{
    const int DEFAULT_RTSP_PORT = 554;

    static const nx_http::MimeProtoVersion rtsp_1_0 = { "RTSP", "1.0" };

    namespace header
    {
        class Range
        {
        public:
            static const nx_http::StringType NAME;
        };
    }

    //!Parses Range header ([rfc2326, 12.29])
    /*!
        Returns range in microseconds
        \note only \a npt is supported
        \note for \a now constant \a DATETIME_NOW is returned
        \warning \a npt is considered to be millis or usec since epoch
    */
    void parseRangeHeader( const nx_http::StringType& rangeStr, qint64* startTime, qint64* endTime );
}

#endif  //NX_RTSP_TYPES_H
