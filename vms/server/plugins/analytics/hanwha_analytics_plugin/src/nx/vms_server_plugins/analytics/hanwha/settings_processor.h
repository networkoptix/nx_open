#pragma once

#include <memory>

#include <QtCore/QString>

#include <nx/utils/url.h>

#include "settings.h"
#include "value_transformer.h"
#include "setting_primitives.h"

namespace nx::vms_server_plugins::analytics::hanwha {

static const FrameSize kDdefaultFrameSize{ 3840, 2160 };

struct DeviceAccessInfo
{
    nx::utils::Url url;
    QString user;
    QString password;
};

class SettingsProcessor
{
private:
    Settings& m_settings;
    std::unique_ptr<ValueTransformer> m_valueTransformer;
    DeviceAccessInfo m_deviceAccessInfo;
    int m_cameraChannelNumber = 0;
    FrameSize m_frameSize;

private:
    void setFrameSize(FrameSize frameSize);

public:
    SettingsProcessor(
        Settings& settings,
        const DeviceAccessInfo& deviceAccessInfo,
        int cameraChannelNumber,
        bool isNvr,
        FrameSize frameSize)
        :
        m_settings(settings),
        m_valueTransformer(isNvr
            ? std::unique_ptr<ValueTransformer>(new NvrValueTransformer(cameraChannelNumber))
            : std::unique_ptr<ValueTransformer>(new CameraValueTransformer)),
        m_deviceAccessInfo(deviceAccessInfo),
        m_cameraChannelNumber(cameraChannelNumber)
    {
        setFrameSize(frameSize);
    }

    // Low level operations.

    std::string makeReadingRequestToDeviceSync(const char* domain, const char* submenu,
        const char* action, bool useChannel = true) const;

    std::string makeWritingRequestToDeviceSync(const std::string& query) const;

    // Mid level operations.

    std::string makeEventTypeReadingRequest(const char* eventTypeInternalName) const;

    // Hi level operations.

    std::optional<QSet<QString>> loadSupportedEventTypes() const;

    std::optional<FrameSize> loadFrameSizeFromDevice() const;
    std::string loadFirmwareVersionFromDevice() const;

    void updateAnalyticsModeOnDevice() const;

    void loadAndHoldSettingsFromDevice();

    void writeSettingsToServer(nx::sdk::SettingsResponse* response) const;

    void transferAndHoldSettingsFromServerToDevice(
        nx::sdk::StringMap* errorMap, const nx::sdk::IStringMap* sourceMap);
};

} // namespace nx::vms_server_plugins::analytics::hanwha
