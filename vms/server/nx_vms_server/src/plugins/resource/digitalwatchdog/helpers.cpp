#include "helpers.h"

#include <nx/fusion/model_functions.h>
#include <plugins/utils/xml_request_helper.h>
#include <plugins/resource/digitalwatchdog/digital_watchdog_resource.h>

namespace {

const std::chrono::seconds kCproApiCacheTimeout(5);

} // namespace

CproApiClient::CproApiClient(QnDigitalWatchdogResource* resource):
    m_resource(resource)
{
    m_cacheExpiration = std::chrono::steady_clock::now();
}

bool CproApiClient::updateVideoConfig()
{
    if (m_cacheExpiration < std::chrono::steady_clock::now())
    {
        nx::plugins::utils::XmlRequestHelper requestHelper(
            m_resource->getUrl(), m_resource->getAuth(), nx::network::http::AuthType::authBasic);
        if (requestHelper.post(lit("GetVideoStreamConfig")))
            m_videoConfig = requestHelper.readRawBody();
        else
            m_videoConfig = std::nullopt;

        m_cacheExpiration = std::chrono::steady_clock::now() + kCproApiCacheTimeout;
    }

    return (bool) m_videoConfig;
}

boost::optional<QStringList> CproApiClient::getSupportedVideoCodecs(Qn::StreamIndex streamIndex)
{
    auto stream = indexOfStream(streamIndex);
    if (stream == -1)
        return boost::none;

    auto types = rangeOfTag("<encodeTypeCaps type=\"list\">", "</encodeTypeCaps>", stream);
    if (!types)
    {
        NX_DEBUG(this, lm("Unable to find %1 stream capabilities on %2")
            .args(QnLexical::serialized(streamIndex), m_resource->getUrl()));
        return boost::none;
    }

    QStringList values;
    while (auto tag = rangeOfTag("<item>", "</item>", types->first, types->second))
    {
        values << QString::fromUtf8(m_videoConfig->mid(tag->first, tag->second));
        types->first = tag->first + tag->second;
    }

    if (values.isEmpty())
    {
        NX_DEBUG(this, lm("Unable to find %1 stream supported codecs on %2")
            .args(QnLexical::serialized(streamIndex), m_resource->getUrl()));
        return boost::none;
    }

    values.sort();
    return values;
}

boost::optional<QString> CproApiClient::getVideoCodec(Qn::StreamIndex streamIndex)
{
    auto stream = indexOfStream(streamIndex);
    if (stream == -1)
        return boost::none;

    auto type = rangeOfTag("<encodeType>", "</encodeType>", stream);
    if (!type)
    {
        NX_DEBUG(this, lm("Unable to find %1 stream codec on %2")
            .args(QnLexical::serialized(streamIndex), m_resource->getUrl()));
        return boost::none;
    }

    return QString::fromUtf8(m_videoConfig->mid(type->first, type->second));
}

bool CproApiClient::setVideoCodec(Qn::StreamIndex streamIndex, const QString &value)
{
    auto stream = indexOfStream(streamIndex);
    if (stream == -1)
        return false;

    auto type = rangeOfTag("<encodeType>", "</encodeType>", stream);
    if (!type)
    {
        NX_DEBUG(this, lm("Unable to find %1 stream codec on %2")
            .args(QnLexical::serialized(streamIndex), m_resource->getUrl()));
        return false;
    }

    NX_DEBUG(this, lm("Set %1 stream codec to %2 on %3")
        .args(QnLexical::serialized(streamIndex), value, m_resource->getUrl()));

    nx::plugins::utils::XmlRequestHelper requestHelper(
        m_resource->getUrl(), m_resource->getAuth(), nx::network::http::AuthType::authBasic);
    m_videoConfig->replace(type->first, type->second, value.toLower().toUtf8());
    return requestHelper.post("SetVideoStreamConfig", *m_videoConfig);
}

int CproApiClient::indexOfStream(Qn::StreamIndex streamIndex)
{
    if (!updateVideoConfig())
        return -1;

    const auto i = m_videoConfig->indexOf(streamIndex == Qn::StreamIndex::primary
        ? "<item id=\"1\"" : "<item id=\"3\"");
    if (i == -1)
    {
        NX_DEBUG(this, lm("Unable to find %1 stream on %2")
            .args(QnLexical::serialized(streamIndex), m_resource->getUrl()));
    }

    return i;
}

boost::optional<std::pair<int, int> > CproApiClient::rangeOfTag(
    const QByteArray &openTag, const QByteArray &closeTag, int rangeBegin, int rangeSize)
{
    auto start = m_videoConfig->indexOf(openTag, rangeBegin);
    if (start == -1 || (rangeSize && start >= rangeBegin + rangeSize))
        return boost::none;
    start += openTag.size();

    auto end = m_videoConfig->indexOf(closeTag, start);
    if (end == -1 || (rangeSize && end >= rangeBegin + rangeSize))
        return boost::none;

    return std::pair<int, int>{start, end - start};
}



