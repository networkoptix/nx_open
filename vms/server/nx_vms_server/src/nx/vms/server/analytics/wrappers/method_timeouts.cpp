#include "method_timeouts.h"

#include <plugins/vms_server_plugins_ini.h>

namespace nx::vms::server::analytics::wrappers {

using namespace std::chrono;
using namespace std::literals::chrono_literals;

static const std::map<SdkMethod, milliseconds> kTimeouts = {
    {SdkMethod::manifest, seconds(pluginsIni().manifestSdkMethodTimeoutS)},
    {SdkMethod::setSettings, seconds(pluginsIni().setSettingsSdkMethodTimeoutS)},
    {SdkMethod::pluginSideSettings, seconds(pluginsIni().pluginSideSettingsSdkMethodTimeoutS)},
    {SdkMethod::setHandler, seconds(pluginsIni().setHandlerSdkMethodTimeoutS)},
    {SdkMethod::createEngine, seconds(pluginsIni().createEngineSdkMethodTimeoutS)},
    {SdkMethod::setEngineInfo, seconds(pluginsIni().setEngineInfoSdkMethodTimeoutS)},
    {SdkMethod::isCompatible, seconds(pluginsIni().isCompatibleSdkMethodTimeoutS)},
    {SdkMethod::obtainDeviceAgent, seconds(pluginsIni().obtainDeviceAgentSdkMethodTimeoutS)},
    {SdkMethod::executeAction, seconds(pluginsIni().executeActionSdkMethodTimeoutS)},
    {
        SdkMethod::setNeededMetadataTypes,
        seconds(pluginsIni().setNeededMetadataTypesSdkMethodTimeoutS)
    },
    {SdkMethod::pushDataPacket, seconds(pluginsIni().pushDataPacketSdkMethodTimeoutS)},
};

milliseconds sdkMethodTimeout(SdkMethod method)
{
    if (const auto it = kTimeouts.find(method); it != kTimeouts.cend())
        return it->second;

    NX_ASSERT(false);
    return milliseconds::zero();
}

} // namespace nx::vms::server::analytics
