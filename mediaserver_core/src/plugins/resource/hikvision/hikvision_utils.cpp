#include <QtXml/QDomElement>

#include "hikvision_utils.h"

#include <nx/network/rtsp/rtsp_types.h>

namespace nx {
namespace mediaserver_core {
namespace plugins {
namespace hikvision {

boost::optional<ChannelStatusResponse> parseChannelElement(const QDomElement& channelRoot)
{
    ChannelStatusResponse response;
    bool status = false;

    if (!channelRoot.isNull() && channelRoot.tagName() == lit("TwoWayAudioChannel"))
    {
        auto childParameter = channelRoot.firstChild();
        QDomElement childElement;
        while (!childParameter.isNull())
        {
            childElement = childParameter.toElement();
            if (childElement.isNull())
                continue;

            auto nodeName = childElement.nodeName();

            if (nodeName == lit("id"))
                response.id = childElement.text();
            else if (nodeName == lit("enabled"))
                response.enabled = childElement.text() == lit("true");
            else if (nodeName == lit("audioCompressionType"))
                response.audioCompression = childElement.text();
            else if (nodeName == lit("audioInputType"))
                response.audioInputType = childElement.text();
            else if (nodeName == lit("speakerVolume"))
            {
                response.speakerVolume = childElement.text().toInt(&status);
                if (!status)
                    return boost::none;
            }
            else if (nodeName == lit("noisereduce"))
                response.noiseReduce = childElement.text() == lit("true");
            else if (nodeName == lit("audioSamplingRate"))
            {
                response.sampleRateKHz = childElement.text().toInt(&status);
                if (!status)
                    return boost::none;
            }

            childParameter = childParameter.nextSibling();
        }
    }
    else
    {
        return boost::none;
    }

    return response;
}

QnAudioFormat toAudioFormat(const QString& codecName, int sampleRateKHz)
{
    QnAudioFormat result;
    if (codecName == lit("G.711alaw"))
    {
        result.setSampleRate(8000);
        result.setCodec("ALAW");
    }
    else if (codecName == lit("G.711ulaw"))
    {
        result.setSampleRate(8000);
        result.setCodec("MULAW");
    }
    else if (codecName == lit("G.726"))
    {
        result.setSampleRate(8000);
        result.setCodec("G726");
    }
    else if (codecName == lit("AAC"))
    {
        result.setSampleRate(16000);
        result.setCodec("AAC");
    }
    if (sampleRateKHz > 0)
        result.setSampleRate(sampleRateKHz); //< override default value

    return result;
}

std::vector<ChannelStatusResponse> parseAvailableChannelsResponse(
    nx_http::StringType message)
{
    QDomDocument doc;
    std::vector<ChannelStatusResponse> channels;

    doc.setContent(message);
    auto docElement = doc.documentElement();

    QDomNode node = docElement.firstChild();
    while (!node.isNull())
    {
        QDomElement element = node.toElement();
        if (!element.isNull() && element.tagName() == lit("TwoWayAudioChannel"))
        {
            auto channel = parseChannelElement(element);
            if (channel)
                channels.push_back(channel.get());
        }
        node = node.nextSibling();
    }

    return channels;
}

boost::optional<ChannelStatusResponse> parseChannelStatusResponse(nx_http::StringType message)
{
    QDomDocument doc;

    doc.setContent(message);
    auto element = doc.documentElement();

    return parseChannelElement(element);
}

boost::optional<OpenChannelResponse> parseOpenChannelResponse(nx_http::StringType message)
{
    OpenChannelResponse response;

    QDomDocument doc;
    doc.setContent(message);

    auto element = doc.documentElement();
    if (!element.isNull() && element.tagName() == lit("TwoWayAudioSession"))
    {
        auto sessionIdNode = element.firstChild();
        if (sessionIdNode.isNull() || !sessionIdNode.isElement())
            return boost::none;

        auto sessionId = sessionIdNode.toElement();

        if (sessionId.tagName() != lit("sessionId"))
            return boost::none;

        response.sessionId = sessionId.text();
    }
    else
    {
        return boost::none;
    }

    return response;
}

boost::optional<CommonResponse> parseCommonResponse(nx_http::StringType message)
{
    CommonResponse response;
    QDomDocument doc;
    doc.setContent(message);

    auto element = doc.documentElement();
    if (!element.isNull() && element.tagName() == lit("ResponseStatus"))
    {
        auto params = element.firstChild();
        while (!params.isNull())
        {
            element = params.toElement();
            if (element.isNull())
                continue;

            auto nodeName = element.nodeName();

            if (nodeName == lit("statusCode"))
                response.statusCode = element.text();
            else if (nodeName == lit("statusString"))
                response.statusString = element.text();
            else if (nodeName == lit("subStatusCode"))
                response.subStatusCode = element.text();

            params = params.nextSibling();
        }
    }
    else
    {
        return boost::none;
    }

    return response;
}

bool parseChannelCapabilitiesResponse(
    const nx::Buffer& response,
    ChannelCapabilities* outCapabilities)
{
    NX_ASSERT(outCapabilities, lit("Output capabilities should be provided."));
    if (!outCapabilities)
        return false;

    QDomDocument doc;
    doc.setContent(response);

    auto element = doc.documentElement();
    if (element.isNull() || element.tagName() != kChannelRootElementTag)
        return false;

    return parseVideoElement(
        element.firstChildElement(kVideoElementTag),
        outCapabilities);
}

bool parseVideoElement(const QDomElement& videoElement, ChannelCapabilities* outCapabilities)
{
    if (videoElement.isNull())
        return false;

    bool success = false;
    std::vector<int> resolutionWidths;
    std::vector<int> resolutionHeights;

    for (const auto& videoChannelProperty : kVideoChannelProperties)
    {
        auto propertyElement = videoElement.firstChildElement(videoChannelProperty);
        if (propertyElement.isNull())
            return false;

        auto tag = propertyElement.tagName();
        auto options = propertyElement.attribute(kOptionsAttribute);
        if (options.isEmpty() && tag != kFixedBitrateTag)
            return false;

        if (tag == kVideoCodecTypeTag)
        {
            success = parseCodecList(options, &outCapabilities->codecs);
        }
        else if (tag == kVideoResolutionWidthTag)
        {
            success = parseIntegerList(options, &resolutionWidths);
        }
        else if (tag == kVideoResolutionHeightTag)
        {
            success = parseIntegerList(options, &resolutionHeights);
        }
        else if (tag == kFixedQualityTag)
        {
            success = parseIntegerList(options, &outCapabilities->quality);
        }
        else if (tag == kMaxFrameRateTag)
        {
            auto& fpsList = outCapabilities->fpsInDeviceUnits;
            success = parseIntegerList(options, &fpsList);
            std::sort(fpsList.begin(), fpsList.end(), std::greater<int>());
        }
        else if (tag == kFixedBitrateTag)
        {
            outCapabilities->bitrateRange.first = propertyElement.attribute("min").toInt(&success);
            if (!success)
                return false;
            outCapabilities->bitrateRange.second = propertyElement.attribute("max").toInt(&success);
        }

        if (!success)
            return false;
    }

    success = makeResolutionList(
        resolutionWidths,
        resolutionHeights,
        &outCapabilities->resolutions);

    return success;
}

bool parseCodecList(const QString& raw, std::set<AVCodecID>* outCodecs)
{
    NX_ASSERT(outCodecs, lit("Output codec set should be provided"));
    if (!outCodecs)
        return false;

    auto split = raw.split(L',');
    for (const auto& codecString : split)
    {
        auto itr = kCodecMap.find(codecString.trimmed());
        if (itr == kCodecMap.end())
            continue;

        outCodecs->insert(itr->second);
    }

    return true;
}

bool parseIntegerList(const QString& raw, std::vector<int>* outIntegerList)
{
    NX_ASSERT(outIntegerList, lit("Output vector should be provided."));
    if (!outIntegerList)
        return false;

    bool success = false;
    auto split = raw.split(L',');

    for (const auto& integerString : split)
    {
        int number = integerString.trimmed().toInt(&success);
        if (!success)
            return false;

        outIntegerList->push_back(number);
    }

    return true;
}

bool makeResolutionList(
    const std::vector<int>& widths,
    const std::vector<int>& heights,
    std::vector<QSize>* outResolutions)
{
    NX_ASSERT(outResolutions, lit("Output vector should be provided."));
    if (!outResolutions)
        return false;

    if (widths.size() != heights.size())
        return false;

    for (auto i = 0; i < widths.size(); ++i)
        outResolutions->emplace_back(widths[i], heights[i]);

    std::sort(
        outResolutions->begin(),
        outResolutions->end(),
        [](const QSize& f, const QSize& s) -> bool
        {
            auto fArea = f.width() * f.height();
            auto sArea = s.width() * s.height();

            if (fArea != sArea)
                return fArea > sArea;

            return f.width() > s.width();
        });

    return true;
}

bool parseChannelPropertiesResponse(nx::Buffer& response, ChannelProperties* outChannelProperties)
{
    NX_ASSERT(outChannelProperties, lit("Output channel properties should be provided."));
    if (!outChannelProperties)
        return false;

    QDomDocument doc;
    doc.setContent(response);

    auto element = doc.documentElement();
    if (element.isNull() || element.tagName() != kChannelRootElementTag)
        return false;

    return parseTransportElement(
        element.firstChildElement(kTransportElementTag),
        outChannelProperties);
}

bool parseTransportElement(
    const QDomElement& transportElement,
    ChannelProperties* outChannelProperties)
{
    if (transportElement.isNull())
        return false;

    auto rtspPortNumberElement = transportElement.firstChildElement(kRtspPortNumberTag);
    if (rtspPortNumberElement.isNull())
    {
        outChannelProperties->rtspPort = nx_rtsp::DEFAULT_RTSP_PORT;
        return true;
    }

    bool success = false;
    outChannelProperties->rtspPort = rtspPortNumberElement.text().toInt(&success);
    return success;
}

bool doGetRequest(
    const QUrl& url,
    const QAuthenticator& auth,
    nx::Buffer* outBuffer,
    nx_http::StatusCode::Value* outStatusCode)
{
    NX_ASSERT(outBuffer, lit("Output buffer should be set."));
    if (!outBuffer)
        return false;

    return doRequest(
        url,
        auth,
        nx_http::Method::get,
        /*bufferToSend*/ nullptr,
        outBuffer,
        outStatusCode);
}

bool doPutRequest(
    const QUrl& url,
    const QAuthenticator& auth,
    const nx::Buffer& buffer,
    nx_http::StatusCode::Value* outStatusCode)
{
    return doRequest(
        url,
        auth,
        nx_http::Method::put,
        &buffer,
        /*outResponseBuffer*/ nullptr,
        outStatusCode);
}

bool doRequest(
    const QUrl& url,
    const QAuthenticator& auth,
    const nx_http::Method::ValueType& method,
    const nx::Buffer* bufferToSend,
    nx::Buffer* outResponseBuffer,
    nx_http::StatusCode::Value* outStatusCode)
{
    if (outStatusCode)
        *outStatusCode = nx_http::StatusCode::undefined;

    nx_http::HttpClient httpClient;
    if (!tuneHttpClient(&httpClient, auth))
        return false;

    bool result = false;
    if (method == nx_http::Method::get)
        result = httpClient.doGet(url);
    else if (method == nx_http::Method::put && bufferToSend)
        result = httpClient.doPut(url, kContentType.toUtf8(), *bufferToSend);

    if (!result)
        return false;

    nx::Buffer responseBuffer;
    while (!httpClient.eof())
        responseBuffer.append(httpClient.fetchMessageBodyBuffer());

    nx_http::StatusCode::Value statusCode = (nx_http::StatusCode::Value)httpClient.response()
        ->statusLine.statusCode;

    if (outStatusCode)
        *outStatusCode = statusCode;

    result = nx_http::StatusCode::isSuccessCode(statusCode);
    if (method == nx_http::Method::put && result)
        result = responseIsOk(parseCommonResponse(responseBuffer));

    if (result && outResponseBuffer)
        *outResponseBuffer = std::move(responseBuffer);

    return result;
}

bool tuneHttpClient(nx_http::HttpClient* outHttpClient, const QAuthenticator& auth)
{
    NX_ASSERT(outHttpClient, lit("Output http client should be provided."));
    if (!outHttpClient)
        return false;

    outHttpClient->setSendTimeoutMs(kHttpTimeout.count());
    outHttpClient->setMessageBodyReadTimeoutMs(kHttpTimeout.count());
    outHttpClient->setResponseReadTimeoutMs(kHttpTimeout.count());
    outHttpClient->setUserName(auth.user());
    outHttpClient->setUserPassword(auth.password());

    return true;
}

QString buildChannelNumber(Qn::ConnectionRole role, int channelNumber)
{
    auto hikvisionChannelNumber = QString::number(channelNumber + 1);
    auto streamNumber = role == Qn::ConnectionRole::CR_LiveVideo
        ? kPrimaryStreamNumber
        : kSecondaryStreamNumber;

    return hikvisionChannelNumber + streamNumber;
}

bool codecSupported(AVCodecID codecId, const ChannelCapabilities& channelCapabilities)
{
    return channelCapabilities.codecs.find(codecId) != channelCapabilities.codecs.end();
}

bool responseIsOk(const boost::optional<CommonResponse>& response)
{
    return response
        && response->statusCode == kStatusCodeOk;
}

int ChannelCapabilities::realMaxFps() const
{
    if (fpsInDeviceUnits.empty())
        return 0;

    const auto maxFps = fpsInDeviceUnits.at(0);
    return maxFps > kFpsThreshold ? maxFps / 100 : maxFps;
}

} // namespace hikvision
} // namespace plugins
} // namespace mediaserver_core
} // namespace nx
