#include "stream_params.h"

#include <nx/network/rtsp/rtsp_types.h>
#include <nx/fusion/serialization/lexical_enum.h>
#include <transcoding/ffmpeg_video_transcoder.h>

namespace nx::rtsp {

bool StreamParams::parseCodec(
    const network::http::HttpHeaders& /*headers*/, const UrlParams& urlParams)
{
    m_codec = AV_CODEC_ID_NONE;
    if (urlParams.codec)
        m_codec = urlParams.codec.value();
    return true;
}

bool StreamParams::parsePosition(
    const network::http::HttpHeaders& headers, const UrlParams& urlParams)
{
    m_position = DATETIME_NOW;
    if (urlParams.position)
    {
        m_position = urlParams.position.value();
    }
    else
    {
        const auto rangeStr = network::http::getHeaderValue(headers, "Range");
        if (!rangeStr.isEmpty())
        {
            if (!network::rtsp::parseRangeHeader(rangeStr, &m_position, &m_endPosition))
                m_position = DATETIME_NOW;

            // VLC/ffmpeg sends position 0 as a default value. Interpret it as Live position.
            if (rangeStr.startsWith("npt=") && m_position == 0)
                m_position = DATETIME_NOW;
        }
    }
    return true;
}

bool StreamParams::parseQuality(
    const network::http::HttpHeaders& headers, const UrlParams& urlParams)
{
    m_quality = MEDIA_Quality_High;
    if (urlParams.quality)
    {
        m_quality = urlParams.quality.value();
    }
    else
    {
        QString qualityStr = network::http::getHeaderValue(headers, "x-media-quality");
        if (!qualityStr.isEmpty())
            m_quality = QnLexical::deserialized<MediaQuality>(qualityStr, MEDIA_Quality_High);
    }
    return true;
}

bool StreamParams::parseResolution(
    const network::http::HttpHeaders& headers, const UrlParams& urlParams)
{
    m_resolution = QSize();
    if (urlParams.resolution)
    {
        m_resolution = urlParams.resolution.value();
    }
    else
    {
        QString resolutionStr = network::http::getHeaderValue(headers, "x-resolution");
        if (!resolutionStr.isEmpty())
        {
            m_resolution = rtsp::parseResolution(resolutionStr);
            if (!m_resolution.isValid())
            {
                m_error = QString("Invalid resolution specified: [%1]").arg(resolutionStr);
                return false;
            }
        }
    }
    return true;
}

bool StreamParams::parseRequest(
    const network::http::Request& request, const QString& defaultVideoCodec)
{
    QString method = request.requestLine.method;

    if (method == "DESCRIBE")
    {
        if (!m_urlParams.parse(QUrlQuery(request.requestLine.url.query())))
        {
            m_error = m_urlParams.getParseError();
            return false;
        }
    }

    if (method == "DESCRIBE" || method == "PLAY")
    {
        if (!parseCodec(request.headers, m_urlParams))
            return false;
        if (!parsePosition(request.headers, m_urlParams))
            return false;
        if (!parseQuality(request.headers, m_urlParams))
            return false;
        if (!parseResolution(request.headers, m_urlParams))
            return false;

        // Setup codec if resolution configured
        if (m_resolution.isValid() && m_codec == AV_CODEC_ID_NONE)
            m_codec = findVideoEncoder(defaultVideoCodec);
    }
    return true;
}

} // namespace nx::rtsp
