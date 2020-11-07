#pragma once

#include <memory>

#include <QtCore/QString>

#include <nx/utils/url.h>
#include <nx/sdk/i_device_info.h>
#include <nx/sdk/i_string_map.h>

#include "settings.h"
#include "value_transformer.h"
#include "setting_primitives.h"

namespace nx::vms_server_plugins::analytics::hanwha {

struct DeviceAccessInfo
{
    nx::utils::Url url;
    QString user;
    QString password;
};

//-------------------------------------------------------------------------------------------------

class BasicSettingsProcessor
{
public:
    BasicSettingsProcessor(
        const nx::sdk::IDeviceInfo* deviceInfo,
        bool isNvr)
    :
        m_valueTransformer(isNvr
            ? std::unique_ptr<ValueTransformer>(new NvrValueTransformer(deviceInfo->channelNumber()))
            : std::unique_ptr<ValueTransformer>(new CameraValueTransformer)),
        m_deviceAccessInfo(DeviceAccessInfo{ deviceInfo->url(), deviceInfo->login(), deviceInfo->password() }),
        m_cameraChannelNumber(deviceInfo->channelNumber())
    {
    }

    std::string makeReadingRequestToDeviceSync(const char* domain, const char* submenu,
        const char* action, bool useChannel = true) const;

    std::string makeWritingRequestToDeviceSync(const std::string& query) const;

    std::string makeEventTypeReadingRequest(const char* eventTypeInternalName) const;

    std::string makeOrientationReadingRequest() const;

    std::string makeEventsStatusReadingRequest() const;

    virtual ~BasicSettingsProcessor() = default;

protected:
    std::unique_ptr<ValueTransformer> m_valueTransformer;
    DeviceAccessInfo m_deviceAccessInfo;
    int m_cameraChannelNumber = 0;
};

//-------------------------------------------------------------------------------------------------

class SettingsProcessor: public BasicSettingsProcessor
{
public:
    SettingsProcessor(
        const nx::sdk::IDeviceInfo* deviceInfo,
        bool isNvr,
        Settings& settings,
        RoiResolution& roiResolution)
    :
        BasicSettingsProcessor(deviceInfo, isNvr),
        m_settings(settings),
        m_roiResolution(roiResolution)
    {
    }

private:

    void updateAnalyticsModeOnDevice() const;

    void loadAndHoldFrameRotationFromDevice();

public:
    std::string loadFirmwareVersionFromDevice() const;

    void loadAndHoldExclusiveSettingsFromServer(const nx::sdk::IStringMap* sourceMap);

    void loadAndHoldSettingsFromDevice();

    void transferAndHoldSettingsFromDeviceToServer(nx::sdk::SettingsResponse* response);

    void transferAndHoldSettingsFromServerToDevice(
        nx::sdk::StringMap* errorMap,
        nx::sdk::StringMap* valueMap,
        const nx::sdk::IStringMap* sourceMap);

private:
    Settings& m_settings;
    RoiResolution& m_roiResolution;
};

} // namespace nx::vms_server_plugins::analytics::hanwha
