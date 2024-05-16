// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "device_agent.h"

#include <future>

#include <nx/kit/utils.h>
#include <nx/sdk/analytics/helpers/event_metadata.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_metadata.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/object_track_best_shot_packet.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/vms_server_plugins/analytics/stub/utils.h>

#include "settings.h"

namespace nx::vms_server_plugins::analytics::stub::http_requests {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

namespace {

class RequestCompletionHandler: public nx::sdk::RefCountable<nx::sdk::IUtilityProvider4::IHttpRequestCompletionHandler>
{
public:
    using Result = nx::sdk::Result<nx::sdk::IString*>;

    virtual void execute(Result response) override { m_promise.set_value(std::move(response)); }

    void fillMetadata(nx::sdk::Ptr<EventMetadata> metadata)
    {
        auto result = m_promise.get_future().get();

        if (!result.isOk())
        {
            std::string error;
            if (result.error().errorMessage())
            {
                error = result.error().errorMessage()->str();
                result.error().errorMessage()->releaseRef();
            }

            metadata->setCaption("Request error");
            metadata->setDescription(
                "Error: " + error + ". Code: " + std::to_string((int) result.error().errorCode()));
            return;
        }

        if (!result.value())
        {
            metadata->setCaption("Request response is not filled");
            return;
        }

        std::string response = result.value()->str();
        result.value()->releaseRef();

        metadata->setCaption("HTTP response");
        metadata->setDescription(std::move(response));
    }

private:
    std::promise<Result> m_promise;
};

} // namespace

static const std::string kEventType = "nx.stub.http_requests.Event";

DeviceAgent::DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo):
    ConsumingDeviceAgent(deviceInfo, /*enableOutput*/ true), m_engine(engine)
{
}

DeviceAgent::~DeviceAgent()
{
}

std::string DeviceAgent::manifestString() const
{
    return /*suppress newline*/ 1 + (const char*) R"json(
{
    "eventTypes": [
        {
            "id": ")json" + kEventType + R"json(",
            "name": "Stub plugin HTTP event"
        }
    ]
}
)json";
}

bool DeviceAgent::pushCompressedVideoFrame(const ICompressedVideoPacket* videoPacket)
{
    m_lastFrameTimestampUs = videoPacket->timestampUs();
    if (m_lastFrameTimestampUs < m_lastEventTimestampUs + m_requestContext.periodSeconds*1E6)
        return true;

    auto handler =  nx::sdk::makePtr<RequestCompletionHandler>();
    m_engine->utilityProvider()->sendHttpRequest(
        m_requestContext.domain,
        m_requestContext.url.c_str(),
        m_requestContext.httpMethod.c_str(),
        m_requestContext.mimeType.c_str(),
        m_requestContext.requestBody.c_str(),
        handler);
    m_lastEventTimestampUs = m_lastFrameTimestampUs;

    auto eventMetadataPacket = makePtr<EventMetadataPacket>();
    eventMetadataPacket->setTimestampUs(m_lastFrameTimestampUs);
    eventMetadataPacket->setDurationUs(0);
    const auto eventMetadata = makePtr<EventMetadata>();
    eventMetadata->setTypeId(kEventType);
    eventMetadata->setIsActive(true);
    eventMetadata->setConfidence(1.0);
    handler->fillMetadata(eventMetadata);
    eventMetadataPacket->addItem(eventMetadata.get());
    pushMetadataPacket(eventMetadataPacket.releasePtr());
    return true;
}

void DeviceAgent::doSetNeededMetadataTypes(
    nx::sdk::Result<void>* /*outValue*/,
    const nx::sdk::analytics::IMetadataTypes* /*neededMetadataTypes*/)
{
}

nx::sdk::Result<const nx::sdk::ISettingsResponse*> DeviceAgent::settingsReceived()
{
    std::map<std::string, std::string> settings = currentSettings();

    std::string domain = settings[kHttpDomainVar];
    if (domain == kHttpCloudDomain)
        m_requestContext.domain = nx::sdk::IUtilityProvider4::HttpDomainName::cloud;
    else
        m_requestContext.domain = nx::sdk::IUtilityProvider4::HttpDomainName::vms;

    m_requestContext.httpMethod = nx::kit::utils::toUpper(settings[kHttpMethodVar]);
    m_requestContext.requestBody = settings[kHttpRequestBodyVar];
    m_requestContext.mimeType = settings[kHttpMimeTypeVar];
    m_requestContext.url = settings[kHttpUrlVar];

    std::string timePeriodSeconds = settings[kHttpRequestTimePeriodSeconds];
    if (!timePeriodSeconds.empty())
        m_requestContext.periodSeconds = (int) std::stod(timePeriodSeconds);

    return nullptr;
}

} // namespace nx::vms_server_plugins::analytics::stub::http_requests
