/**********************************************************
* Jul 31, 2015
* a.kolesnikov
***********************************************************/

#include "rtsp_types.h"


namespace nx_rtsp
{
    namespace 
    {
        void extractNptTime( const nx_http::StringType& strValue, qint64* dst )
        {
            if( strValue == "now" )
            {
                //*dst = getRtspTime();
                *dst = DATETIME_NOW;
            }
            else
            {
                double val = strValue.toDouble();
                // some client got time in seconds, some in microseconds, convert all to microseconds
                *dst = val < 1000000 ? val * 1000000.0 : val;
            }
        }
    }

    namespace header
    {
        const nx_http::StringType Range::NAME = "Range";
    }

    void parseRangeHeader( const nx_http::StringType& rangeStr, qint64* startTime, qint64* endTime )
    {
        const auto rangeType = rangeStr.trimmed().split('=');
        if (rangeType.size() < 2)
            return;
        if (rangeType[0] == "npt")
        {
            const auto values = rangeType[1].split('-');
        
            extractNptTime(values[0], startTime);
            if (values.size() > 1 && !values[1].isEmpty())
                extractNptTime(values[1], endTime);
            else
                *endTime = DATETIME_NOW;
        }
    }
}
