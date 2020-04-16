#include "device_agent.h"

#include <stdexcept>
#include <string>

#define NX_PRINT_PREFIX (m_logUtils.printPrefix)
#include <nx/kit/debug.h>
#include <nx/kit/json.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/plugin_diagnostic_event.h>
#include <nx/utils/url.h>

#include "ini.h"
#include "add_ptr_ref.h"
#include "object_types.h"
#include "parse_object_metadata_packet.h"

namespace nx::vms_server_plugins::analytics::vivotek {

using namespace std::literals;
using namespace nx::kit;
using namespace nx::sdk;
using namespace nx::sdk::analytics;
using namespace nx::network;
using namespace nx::network::websocket;

namespace {

const auto kReopenDelay = 10s;

const std::string kNewTrackEventType = "nx.vivotek.newTrack";
const std::string kHelloWorldObjectType = "nx.vivotek.helloWorld";

std::string makePrintPrefix(const IDeviceInfo* deviceInfo)
{
    return "[" + libContext().name() + "_device" +
        (!deviceInfo ? "" : (std::string("_") + deviceInfo->id())) + "] ";
}

} // namespace

DeviceAgent::DeviceAgent(const nx::sdk::IDeviceInfo* deviceInfo):
    m_logUtils(NX_DEBUG_ENABLE_OUTPUT, makePrintPrefix(deviceInfo)),
    m_url(deviceInfo->url())
{
    NX_OUTPUT << __func__;

    m_url.setScheme("");
    m_url.setUserName(deviceInfo->login());
    m_url.setPassword(deviceInfo->password());
    m_url.setPath("");
    m_url.setQuery(QUrlQuery());
    m_url.setFragment("");

    NX_OUTPUT << "    m_url: " << m_url.toStdString();
}

void DeviceAgent::doSetSettings(Result<const IStringMap*>* /*outResult*/, const IStringMap* settings)
{
    NX_OUTPUT << __func__;

    executeInAioThreadSync(
        [&]{
            for (int i = 0; i < settings->count(); ++i)
                NX_OUTPUT << "    " << settings->key(i) << ": " << settings->value(i);
        });
}

void DeviceAgent::getPluginSideSettings(Result<const ISettingsResponse*>* /*outResult*/) const
{
    NX_OUTPUT << __func__;
}

void DeviceAgent::getManifest(Result<const IString*>* outResult) const
{
    NX_OUTPUT << __func__;

    *outResult = new sdk::String(Json(Json::object{
        {"objectTypes", Json::array{
            Json::object{
                {"id", kObjectTypes.person},
                {"name", "Person"},
            },
        }},
        {"deviceAgentSettingsModel", Json::object{
            {"type", "Settings"},
            {"sections", Json::array{
                Json::object{
                    {"type", "Section"},
                    {"caption", "Deep Learning VCA"},
                    {"items", Json::array{
                        Json::object{
                            {"name", "Mount.Height"},
                            {"type", "SpinBox"},
                            {"caption", "Camera height (cm)"},
                            {"description", "Distance between camera and floor"},
                            {"minValue", 0},
                            {"maxValue", 2000},
                        },
                        Json::object{
                            {"name", "Mount.TiltAngle"},
                            {"type", "SpinBox"},
                            {"caption", "Camera tilt angle (°)"},
                            {"description", "Angle between camera direction vector and down vector"},
                            {"minValue", 0},
                            {"maxValue", 179},
                        },
                    }},
                },
            }},
        }},
    }).dump());

    NX_OUTPUT << "    " << outResult->value()->str();
}

void DeviceAgent::setHandler(IHandler* handler)
{
    NX_OUTPUT << __func__;

    executeInAioThreadSync(
        [&]{
            m_handler = addPtrRef(handler);
        });
}

void DeviceAgent::doSetNeededMetadataTypes(Result<void>* /*outResult*/, const IMetadataTypes* neededMetadataTypes)
{
    NX_OUTPUT << __func__;

    executeInAioThreadSync(
        [&]{
            NX_OUTPUT << "    eventTypeIds:";
            const auto eventTypeIds = neededMetadataTypes->eventTypeIds();
            for (int i = 0; i < eventTypeIds->count(); ++i)
                NX_OUTPUT << "        " << eventTypeIds->at(i);
            NX_OUTPUT << "    objectTypeIds:";
            const auto objectTypeIds = neededMetadataTypes->objectTypeIds();
            for (int i = 0; i < objectTypeIds->count(); ++i)
                NX_OUTPUT << "        " << objectTypeIds->at(i);

            const bool wantMetadata = !neededMetadataTypes->isEmpty();
            if (m_wantMetadata == wantMetadata)
                return;

            m_wantMetadata = wantMetadata;

            reopenNativeMetadataSource();
        });
}

void DeviceAgent::emitDiagnostic(
    IPluginDiagnosticEvent::Level level,
    const QString& caption, const QString& description)
{
    const auto event = makePtr<PluginDiagnosticEvent>(
        level, caption.toStdString(), description.toStdString());

    m_handler->handlePluginDiagnosticEvent(event.get());
}

void DeviceAgent::reopenNativeMetadataSource()
{
    NX_OUTPUT << __func__;

    m_reopenDelayer.cancelAsync(
        [this, self = addPtrRef(this)]() mutable
        {
            NX_OUTPUT << "    reopenDelayer cancelled";

            m_nativeMetadataSource.close(
                [this, self = std::move(self)]() mutable
                {
                    NX_OUTPUT << "    nativeMetadataSource closed";

                    if (!m_wantMetadata)
                        return;

                    m_nativeMetadataSource.open(m_url,
                        [this, self = std::move(self)](auto exceptionPtr) mutable
                        {
                            try
                            {
                                if (exceptionPtr)
                                    std::rethrow_exception(exceptionPtr);

                                NX_OUTPUT << "    nativeMetadataSource opened";

                                readNextMetadata();
                            }
                            catch (std::exception const& exception)
                            {
                                emitDiagnostic(IPluginDiagnosticEvent::Level::error,
                                    "Failed to open metadata source", exception.what());

                                m_reopenDelayer.start(kReopenDelay,
                                    [this, self = std::move(self)]()
                                    {
                                        reopenNativeMetadataSource();
                                    });
                            }
                        });
                });
        });
}

void DeviceAgent::readNextMetadata()
{
    m_nativeMetadataSource.read(
        [this, self = addPtrRef(this)](auto exceptionPtr, Json nativeMetadata) mutable
        {
            try
            {
                if (exceptionPtr)
                    std::rethrow_exception(exceptionPtr);

                if (auto packet = parseObjectMetadataPacket(nativeMetadata))
                    m_handler->handleMetadata(packet.releasePtr());

                readNextMetadata();
            }
            catch (std::exception const& exception)
            {
                emitDiagnostic(IPluginDiagnosticEvent::Level::error,
                    "Failed to read metadata", exception.what());

                reopenNativeMetadataSource();
            }
        });
}

} // namespace nx::vms_server_plugins::analytics::vivotek
