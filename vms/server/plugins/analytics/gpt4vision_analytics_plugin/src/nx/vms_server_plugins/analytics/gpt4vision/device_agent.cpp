// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <chrono>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
} // extern "C"

#include <QtCore/QByteArray>
#include <QtCore/QScopedValueRollback>
#include <QtCore/QString>

#include <nx/kit/debug.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/http/http_async_client.h>
#include <nx/network/http/http_types.h>
#include <nx/reflect/instrument.h>
#include <nx/reflect/json.h>
#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>
#include <nx/sdk/analytics/helpers/pixel_format.h>
#include <nx/sdk/helpers/error.h>
#include <nx/utils/scope_guard.h>

#include "engine.h"
#include "ini.h"

namespace nx::vms_server_plugins::analytics::gpt4vision {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace {

constexpr auto kOpenAiApiPath = "https://api.openai.com/v1/chat/completions";

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
    if (m_client)
        m_client->pleaseStopSync();
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

    auto imageUrl = encodeImage(videoFrame);
    if (imageUrl.url.empty())
        return true;

    open_ai::QueryPayload payload{.messages = {
        {.content = {
            {.type = "text", .text = m_queryText},
            {.type = "image_url", .image_url = std::move(imageUrl)}
        }}
    }};

    m_lastQueryTimestamp = videoFrameTimestamp;
    m_queryInProgress = true;
    sendVideoFrameToOpenAi(payload);
    return true; //< There were no errors while processing the video frame.
}

open_ai::QueryPayload::Message::Content::ImageUrl DeviceAgent::encodeImage(
    const IUncompressedVideoFrame* videoFrame,
    std::string format) const
{
    // TODO: #sivanov Support other frame pixel formats.
    if (videoFrame->pixelFormat() != IUncompressedVideoFrame::PixelFormat::yuv420)
    {
        showErrorMessage(NX_FMT(
            "Internal plugin error: unexpected frame format %1.",
            (int) videoFrame->pixelFormat()
        ).toStdString());
        return {};
    }

    const AVCodec* codec = avcodec_find_encoder_by_name(
        (format == "jpg" || format == "jpeg") ? "mjpeg" : format.c_str());
    if (!codec)
    {
        showErrorMessage(NX_FMT(
            "Internal plugin error: codec %1 cannot be found.",
            format
        ).toStdString());
        return {};
    }

    AVCodecContext* context = avcodec_alloc_context3(codec);
    if (!context)
    {
        showErrorMessage("Internal plugin error: cannot initialize encoder context.");
        return {};
    }
    auto contextGuard = nx::utils::makeScopeGuard([&context]() {avcodec_free_context(&context); });

    context->pix_fmt = AV_PIX_FMT_YUVJ420P;
    context->width = videoFrame->width();
    context->height = videoFrame->height();
    context->bit_rate = videoFrame->width() * videoFrame->height();
    context->time_base.num = 1;
    context->time_base.den = 30;

    if (int error = avcodec_open2(context, codec, nullptr); error < 0)
    {
        showErrorMessage(NX_FMT(
            "Internal plugin error %1: cannot initialize encoder %2 to encode image of %3x%4.",
            error,
            format,
            videoFrame->width(),
            videoFrame->height()
        ).toStdString());
        return {};
    }
    auto codecGuard = nx::utils::makeScopeGuard([context]() {avcodec_close(context); });

    AVPacket* packet = av_packet_alloc();
    if (!packet)
    {
        showErrorMessage("Internal plugin error: cannot allocate ffmpeg packet.");
        return {};
    }
    auto packetGuard = nx::utils::makeScopeGuard([&packet]() {av_packet_free(&packet); });

    AVFrame* frame = av_frame_alloc();
    if (!frame)
    {
        showErrorMessage("Internal plugin error: cannot allocate video frame.");
        return {};
    }
    auto frameGuard = nx::utils::makeScopeGuard([&frame]() {av_frame_free(&frame); });
    frame->width = videoFrame->width();
    frame->height = videoFrame->height();
    frame->format = AV_PIX_FMT_YUVJ420P;
    for (int i = 0; i < videoFrame->planeCount(); ++i)
    {
        frame->linesize[i] = videoFrame->lineSize(i);
        frame->data[i] = (uint8_t*) (videoFrame->data(i));
    }

    if (int error = avcodec_send_frame(context, frame); error < 0)
    {
        showErrorMessage(NX_FMT(
            "Internal plugin error %1: cannot use %2 to encode image of %3x%4.",
            error,
            format,
            videoFrame->width(),
            videoFrame->height()
        ).toStdString());
    }

    if (int error = avcodec_receive_packet(context, packet); error < 0)
    {
        showErrorMessage(NX_FMT(
            "Internal plugin error %1: cannot use %2 to encode image of %3x%4.",
            error,
            format,
            videoFrame->width(),
            videoFrame->height()
        ).toStdString());
        return {};
    }

    QByteArray buffer;
    buffer.append((const char*) packet->data, packet->size);
    std::string imageData = buffer.toBase64().toStdString();
    av_packet_unref(packet);
    return {.url = "data:image/" + format + ";base64," + std::move(imageData)};
}

void DeviceAgent::sendVideoFrameToOpenAi(const open_ai::QueryPayload& payload)
{
    using namespace nx::network::http;

    auto messageBody = std::make_unique<BufferSource>(
        header::ContentType::kJson,
        nx::reflect::json::serialize(payload));

    auto completionHandler =
        [this]()
        {
            if (m_client->response())
            {
                const std::string result = m_client->fetchMessageBodyBuffer().takeStdString();
                const auto [response, success] =
                    nx::reflect::json::deserialize<open_ai::Response>(result);
                if (success)
                {
                    handleOpenAiResponse(response);
                }
                else
                {
                    showErrorMessage("Response cannot be deserialized:\n"
                        + nx::kit::utils::toString(result));
                }
            }
            else
            {
                showErrorMessage("Request cannot be sent: "
                    + SystemError::toString(m_client->lastSysErrorCode()));
            }
            m_queryInProgress = false;
        };

    m_client = std::make_unique<AsyncClient>(nx::network::ssl::kDefaultCertificateCheck);
    m_client->setOnDone(completionHandler);
    m_client->setCredentials(BearerAuthToken{m_engine->apiKey()});
    m_client->setRequestBody(std::move(messageBody));
    m_client->setTimeouts(AsyncClient::kInfiniteTimeouts);
    m_client->doPost(kOpenAiApiPath);
}

void DeviceAgent::handleOpenAiResponse(const open_ai::Response& response)
{
    if (!response.choices.empty())
    {
        auto eventMetadataPacket = makePtr<EventMetadataPacket>();
        eventMetadataPacket->setTimestampUs(m_lastQueryTimestamp.count());
        eventMetadataPacket->setDurationUs(0);

        const auto eventMetadata = makePtr<EventMetadata>();
        eventMetadata->setTypeId(kEventType);
        eventMetadata->setIsActive(true);

        // It is better to put long texts in description as it simplifies Bookmark creation
        // and other automatic processing.
        eventMetadata->setCaption("GPT4");
        eventMetadata->setDescription(response.choices[0].message.content);
        eventMetadataPacket->addItem(eventMetadata.get());
        pushMetadataPacket(eventMetadataPacket.releasePtr());
    }
    else if (!response.error.message.empty())
    {
        showErrorMessage(response.error.message);
    }
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

void DeviceAgent::showErrorMessage(const std::string& message) const
{
    pushPluginDiagnosticEvent(IPluginDiagnosticEvent::Level::error, "GPT 4 Vision Error", message);
}

} // namespace nx::vms_server_plugins::analytics::gpt4vision
