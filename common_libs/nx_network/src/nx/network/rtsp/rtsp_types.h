#pragma once

#include <nx/network/http/http_types.h>

namespace nx_rtsp {

NX_NETWORK_API extern const char* const kUrlSchemeName;
NX_NETWORK_API extern const char* const kSecureUrlSchemeName;

const int DEFAULT_RTSP_PORT = 554;

static const nx_http::MimeProtoVersion rtsp_1_0 = { "RTSP", "1.0" };

namespace header {

class NX_NETWORK_API Range
{
public:
    static const nx_http::StringType NAME;
};

} // namespace rtsp

class NX_NETWORK_API RtspResponse:
    public nx_http::Response
{
public:
    bool parse(const nx_http::ConstBufferRefType& data);
};

/**
 * Parses Range header ([rfc2326, 12.29]).
 * Returns range in microseconds.
 * NOTE: Only clock is supported.
 *   Though, it MUST contain UTC timestamp (millis or usec). I.e., not rfc2326-compliant.
 * NOTE: For now constant DATETIME_NOW is returned.
 * @return true if startTime and endTime were filled with values.
*/
bool NX_NETWORK_API parseRangeHeader(const nx_http::StringType& rangeStr, qint64* startTime, qint64* endTime);

} // namespace nx_rtsp
