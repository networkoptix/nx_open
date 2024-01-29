// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <chrono>

#include <QtCore/QBuffer>
#include <QtCore/QScopedValueRollback>
#include <QtCore/QString>
#include <QtGui/QImage>

#include <nx/kit/debug.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/http_types.h>
#include <nx/reflect/instrument.h>
#include <nx/reflect/json.h>
#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>
#include <nx/sdk/analytics/helpers/pixel_format.h>
#include <nx/sdk/helpers/error.h>

#include "engine.h"
#include "ini.h"

namespace nx::vms_server_plugins::analytics::gpt4vision {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace {

constexpr auto kOpenAiApiPath = "https://api.openai.com/v1/chat/completions";

open_ai::QueryPayload::Message::Content::ImageUrl encodeImage(
    const IUncompressedVideoFrame* videoFrame,
    std::string format = "jpg")
{
    QBuffer buffer;
    buffer.open(QIODevice::WriteOnly);

    QImage img(
        (uchar*) videoFrame->data(0),
        videoFrame->width(),
        videoFrame->height(),
        QImage::Format::Format_RGB888);

    img.save(&buffer, format.c_str());
    std::string imageData = buffer.data().toBase64().toStdString();
    return {.url = "data:image/" + format + ";base64," + std::move(imageData)};
}

struct Manifest
{
    struct SupportedType
    {
        std::string eventTypeId;
    };

    struct TypeLibrary
    {
        struct EventType
        {
            std::string id;
            std::string name;
        };
        std::vector<EventType> eventTypes;
    };

    std::vector<SupportedType> supportedTypes;
    TypeLibrary typeLibrary;
};
NX_REFLECTION_INSTRUMENT(Manifest::SupportedType, (eventTypeId))
NX_REFLECTION_INSTRUMENT(Manifest::TypeLibrary::EventType, (id)(name))
NX_REFLECTION_INSTRUMENT(Manifest::TypeLibrary, (eventTypes))
NX_REFLECTION_INSTRUMENT(Manifest, (supportedTypes)(typeLibrary))

} // namespace

DeviceAgent::DeviceAgent(const Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo):
    ConsumingDeviceAgent(deviceInfo, ini().enableOutput),
    m_engine(engine)
{
}

DeviceAgent::~DeviceAgent()
{
}

std::string DeviceAgent::manifestString() const
{
    static const Manifest manifest{
        .supportedTypes{{kEventType}},
        .typeLibrary{
            .eventTypes{
                {.id = kEventType, .name = "GPT4 Vision"}
            }
        }
    };
    return nx::reflect::json::serialize(manifest);
}

/**
 * Called when the Server sends a new uncompressed frame from a camera.
 */
bool DeviceAgent::pushUncompressedVideoFrame(const IUncompressedVideoFrame* videoFrame)
{
    if (m_engine->apiKey().empty())
        return true;

    if (m_queryInProgress)
        return true;

    const auto videoFrameTimestamp = std::chrono::microseconds(videoFrame->timestampUs());
    if (videoFrameTimestamp < m_lastQueryTimestamp + m_queryPeriod)
        return true;

    m_lastQueryTimestamp = videoFrameTimestamp;

    open_ai::QueryPayload payload{.messages = {
        {.content = {
            {.type = "text", .text = m_queryText},
            {.type = "image_url", .image_url = encodeImage(videoFrame)}
        }}
    }};

    if (const auto response = sendVideoFrameToOpenAi(payload))
    {
        if (!response->choices.empty())
        {
            auto eventMetadataPacket = makePtr<EventMetadataPacket>();
            eventMetadataPacket->setTimestampUs(videoFrame->timestampUs());
            eventMetadataPacket->setDurationUs(0);

            const auto eventMetadata = makePtr<EventMetadata>();
            eventMetadata->setTypeId(kEventType);
            eventMetadata->setIsActive(true);

            // It is better to put long texts in description as it simplifies Bookmark creation
            // and other automatic processing.
            eventMetadata->setCaption("GPT4");
            eventMetadata->setDescription(response->choices[0].message.content);
            eventMetadataPacket->addItem(eventMetadata.get());
            pushMetadataPacket(eventMetadataPacket.releasePtr());
        }
        else if (!response->error.message.empty())
        {
            showErrorMessage(response->error.message);
        }
    }

    return true; //< There were no errors while processing the video frame.
}

std::optional<open_ai::Response> DeviceAgent::sendVideoFrameToOpenAi(
    const open_ai::QueryPayload& payload)
{
    using namespace nx::network::http;

    QScopedValueRollback<bool> guard(m_queryInProgress, true);

    auto messageBody = std::make_unique<BufferSource>(
        header::ContentType::kJson,
        nx::reflect::json::serialize(payload));

    auto client = std::make_unique<HttpClient>(nx::network::ssl::kDefaultCertificateCheck);
    client->setCredentials(BearerAuthToken{m_engine->apiKey()});
    client->setTimeouts(AsyncClient::kInfiniteTimeouts);
    const bool responseReceived = client->doPost(
        kOpenAiApiPath,
        std::move(messageBody));

    if (responseReceived)
    {
        const std::string result = client->fetchEntireMessageBody()->takeStdString();

        const auto [response, success] = nx::reflect::json::deserialize<open_ai::Response>(result);
        if (success)
            return response;

        showErrorMessage("Response cannot be deserialized:\n" + result);
    }
    else
    {
        showErrorMessage("Request cannot be sent: " + client->lastSysErrorCode());
    }
    return std::nullopt;
}

void DeviceAgent::doSetNeededMetadataTypes(
    nx::sdk::Result<void>* /*outValue*/,
    const nx::sdk::analytics::IMetadataTypes* /*neededMetadataTypes*/)
{
}

nx::sdk::Result<const nx::sdk::ISettingsResponse*> DeviceAgent::settingsReceived()
{
    std::map<std::string, std::string> settings = currentSettings();

    m_queryText = settings[std::string(kQueryParameter)];
    const auto timePeriodSeconds = QString::fromStdString(settings[std::string(kPeriodParameter)]);
    m_queryPeriod = static_cast<std::chrono::seconds>(timePeriodSeconds.toInt());
    if (m_queryPeriod <= 0s)
    {
        m_queryPeriod = kDefaultQueryPeriod;
        return error(ErrorCode::invalidParams,
            QString("Time period is invalid. Default value of %1 seconds will be used.")
                .arg(kDefaultQueryPeriod.count()).toStdString());
    }

    return nullptr;
}

void DeviceAgent::showErrorMessage(const std::string& message)
{
    pushPluginDiagnosticEvent(IPluginDiagnosticEvent::Level::error, "GPT 4 Vision Error", message);
}

} // namespace nx::vms_server_plugins::analytics::gpt4vision
