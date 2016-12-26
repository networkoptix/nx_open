#include <QtXml/QDomElement>

#include "hikvision_parsing_utils.h"

namespace nx {
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
    bool status;

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
    bool status;

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

} // namespace hikvision
} // namespace plugins
} // namespace nx