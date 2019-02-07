#include "helpers.h"
#if defined(ENABLE_ONVIF)

#include <nx/network/url/url_builder.h>
#include <nx/network/http/http_client.h>

#include <nx/fusion/model_functions.h>
#include <plugins/utils/xml_request_helper.h>
#include <plugins/resource/digitalwatchdog/digital_watchdog_resource.h>

namespace {

const std::chrono::seconds kCproApiCacheTimeout(5);

const char* kJsonApiRequestDataTemplate =
    R"json({"jsonData":{"data":%1,"file":"param","password":"%2","username":"%3"}})json";

QSize jsonApiResolutionToQSize(QString resolution)
{
    if (resolution == "vga")
        return QSize(640, 480);
    if (resolution == "cif")
        return QSize(352, 240);
    if (resolution == "4cif")
        return QSize(704, 480);
    if (resolution == "d1")
        return QSize(720, 480);
    if (resolution == "720p")
        return QSize(1280, 720);
    if (resolution == "1080p")
        return QSize(1920, 1080);

    const auto sizeComponents = resolution.split('x');
    if (sizeComponents.size() != 2)
    {
        NX_DEBUG(typeid(QJsonObject), "Unknown resolution string format");
        return {};
    }

    return QSize(sizeComponents[0].toInt(), sizeComponents[1].toInt());
}

QString QSizeToJsonApiResolution(QSize resolution)
{
    return QString::number(resolution.width()) + "x" + QString::number(resolution.height());
}

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

boost::optional<QStringList> CproApiClient::getSupportedVideoCodecs(
    nx::vms::api::MotionStreamType streamIndex)
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

boost::optional<QString> CproApiClient::getVideoCodec(nx::vms::api::MotionStreamType streamIndex)
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

bool CproApiClient::setVideoCodec(nx::vms::api::MotionStreamType streamIndex, const QString &value)
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

int CproApiClient::indexOfStream(nx::vms::api::MotionStreamType streamIndex)
{
    if (!updateVideoConfig())
        return -1;

    const auto i = m_videoConfig->indexOf(streamIndex == nx::vms::api::MotionStreamType::primary
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

/* ============================================================================================== */

JsonApiClient::JsonApiClient(nx::network::SocketAddress address, QAuthenticator auth):
    m_address(std::move(address)),
    m_auth(std::move(auth))
{
}

nx::vms::server::resource::StreamCapabilityMap JsonApiClient::getSupportedVideoCodecs(
    int channelNumber, nx::vms::api::MotionStreamType streamIndex)
{
    NX_ASSERT(streamIndex != nx::vms::api::MotionStreamType::undefined);

    ++channelNumber; //< Camera API channel numbers start with 1.
    const QJsonObject params = getParams(QString("All.VideoInput._%1._%2")
        .arg(channelNumber).arg(streamIndex == nx::vms::api::MotionStreamType::primary ? 1 : 2));

    if (params.isEmpty())
        return {};

    const QStringList codecs = params["Codec"]["PVALUES"].toString().split(',');
    nx::vms::server::resource::StreamCapabilityMap result;
    for (const auto& codec: codecs)
    {
        const auto resolutions = params[codec]["Resolution"]["PVALUES"].toString().split(',');
        for (const auto& resolution: resolutions)
        {
            nx::vms::server::resource::StreamCapabilityKey key;
            key.codec = codec.toUpper();
            key.resolution = jsonApiResolutionToQSize(resolution);
            result.insert(key, nx::media::CameraStreamCapability());
        }
    }

    return result;
}

bool JsonApiClient::sendStreamParams(
    int channelNumber,
    nx::vms::api::MotionStreamType streamIndex,
    const QnLiveStreamParams& streamParams)
{
    NX_ASSERT(streamIndex != nx::vms::api::MotionStreamType::undefined);

    ++channelNumber; //< Camera API channel numbers start with 1.
    QString paramBasename =  QString("All.VideoInput._%1._%2.")
        .arg(channelNumber).arg(streamIndex == nx::vms::api::MotionStreamType::primary ? 1 : 2);

    const QString codec = streamParams.codec.toLower();
    const auto response = setParams({
        {paramBasename + "Codec", codec},
        {paramBasename + codec + ".Cbr", QString::number(streamParams.bitrateKbps)},
        {paramBasename + codec + ".FrameRate", QString::number(int(streamParams.fps))},
        {paramBasename + codec + ".Resolution", QSizeToJsonApiResolution(streamParams.resolution)}
    });

    if (response.isEmpty())
        return false;
    return true;
}

QJsonObject JsonApiClient::getParams(QString paramName)
{
    const auto requestUrl = nx::network::url::Builder()
        .setScheme(nx::network::http::kUrlSchemeName)
        .setEndpoint(m_address)
        .setPath("/cgi-bin/GetJsonValue.cgi")
        .setQuery("TYPE=json")
        .toUrl();

    const auto response = doRequest(requestUrl,
        lm(kJsonApiRequestDataTemplate)
            .args('"' + paramName + '"', m_auth.password(), m_auth.user()).toUtf8());
    if (!response.has_value())
        return {};

    NX_VERBOSE(this, "Response: [%1]", *response);
    return *response;
}

QJsonObject JsonApiClient::setParams(const std::map<QString, QString>& keyValueParams)
{
    const auto requestUrl = nx::network::url::Builder()
        .setScheme(nx::network::http::kUrlSchemeName)
        .setEndpoint(m_address)
        .setPath("/cgi-bin/SetJsonValue.cgi")
        .setQuery("TYPE=json")
        .toUrl();

    QJsonObject paramsJson;
    for (const auto&[param, value]: keyValueParams)
        paramsJson.insert(param, value.toLower());
    QString params = QJsonDocument(paramsJson).toJson(QJsonDocument::Compact);

    const auto response = doRequest(requestUrl,
        lm(kJsonApiRequestDataTemplate).args(std::move(params), m_auth.password(), m_auth.user()).toUtf8());
    if (!response.has_value())
        return {};

    NX_VERBOSE(this, "Response: [%1]", *response);
    return *response;
}

std::optional<QJsonObject> JsonApiClient::doRequest(const nx::utils::Url& url, QByteArray data)
{
    NX_VERBOSE(this, "Sending request [%1] with data [%2]", url, data);

    nx::network::http::HttpClient client;
    if (!client.doPost(url, "application/json", std::move(data)))
    {
        NX_DEBUG(this, "Error posting request [%1]: errno: [%2]",
            url, SystemError::getLastOSErrorText());
        return {};
    }

    const auto httpResponse = client.response();
    NX_ASSERT(httpResponse);
    if (!httpResponse)
        return {};

    if (!nx::network::http::StatusCode::isSuccessCode(httpResponse->statusLine.statusCode))
    {
        NX_DEBUG(this, "Error with request [%1]: HTTP code: [%2]",
            url, httpResponse->statusLine.statusCode);
        return {};
    }

    const auto response = client.fetchEntireMessageBody();
    if (!response)
    {
        NX_VERBOSE(this, "Error fetching value for request [%1]", url);
        return {};
    }

    QJsonParseError error;
    const auto jsonDoc = QJsonDocument::fromJson(*response, &error);
    if (jsonDoc.isNull() || !jsonDoc.isObject())
    {
        NX_VERBOSE(this, "Error parsing JSON response: [%1]; [%2]", error.errorString(), response);
        return {};
    }
    return jsonDoc.object();
}

#endif // defined(ENABLE_ONVIF)
