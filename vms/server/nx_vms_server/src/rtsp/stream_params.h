#pragma once

#include <rtsp/rtsp_utils.h>
#include <nx/network/http/http_types.h>

namespace nx::rtsp {

class StreamParams {
public:
    bool parseRequest(const network::http::Request& request, const QString& defaultVideoCodec);
    QString getParseError() { return m_error; }

    QSize resolution() { return m_resolution; }
    int64_t position() { return m_position; }
    int64_t endPosition() { return m_endPosition; }
    MediaQuality quality() { return m_quality; }
    AVCodecID videoCodec() { return m_codec; }

private:
    bool parseCodec(const network::http::HttpHeaders& headers, const UrlParams& urlParams);
    bool parsePosition(const network::http::HttpHeaders& headers, const UrlParams& urlParams);
    bool parseQuality(const network::http::HttpHeaders& headers, const UrlParams& urlParams);
    bool parseResolution(const network::http::HttpHeaders& headers, const UrlParams& urlParams);

private:
    int64_t m_position = DATETIME_NOW;
    int64_t m_endPosition = 0;
    MediaQuality m_quality = MEDIA_Quality_High;
    AVCodecID m_codec = AV_CODEC_ID_NONE;
    QSize m_resolution;

private:
    UrlParams m_urlParams;
    QString m_error;
};

} // namespace nx::rtsp
