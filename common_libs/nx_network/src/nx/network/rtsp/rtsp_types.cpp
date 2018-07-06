#include "rtsp_types.h"

#include <nx/utils/datetime.h>

namespace nx_rtsp {

const char* const kUrlSchemeName = "rtsp";
const char* const kSecureUrlSchemeName = "rtsps";

namespace {

void extractNptTime(const nx::network::http::StringType& strValue, qint64* dst)
{
    if (strValue == "now")
    {
        //*dst = getRtspTime();
        *dst = DATETIME_NOW;
    }
    else
    {
        double val = strValue.toDouble();
        // Some client got time in seconds, some in microseconds, convert all to microseconds.
        *dst = val < 1000000 ? val * 1000000.0 : val;
    }
}

} // namespace

namespace header {

const nx::network::http::StringType Range::NAME = "Range";

} // namespace header

bool parseRangeHeader(
    const nx::network::http::StringType& rangeStr,
    qint64* startTime,
    qint64* endTime)
{
    const auto rangeType = rangeStr.trimmed().split('=');
    if (rangeType.size() != 2)
        return false;
    if (rangeType[0] == "clock")
    {
        const auto values = rangeType[1].split('-');
        if (values.isEmpty() || values.size() > 2)
            return false;

        extractNptTime(values[0], startTime);
        if (values.size() > 1 && !values[1].isEmpty())
            extractNptTime(values[1], endTime);
        else
            *endTime = DATETIME_NOW;
        return true;
    }

    return false;
}

bool RtspResponse::parse(const nx::network::http::ConstBufferRefType &data)
{
    return nx::network::http::parseRequestOrResponse(
        data,
        (nx::network::http::Response*)this,
        &nx::network::http::Response::statusLine,
        true);
}

} // namespace nx_rtsp
