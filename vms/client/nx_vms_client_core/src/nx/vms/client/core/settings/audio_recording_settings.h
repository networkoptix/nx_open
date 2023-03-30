// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/property_storage/storage.h>
#include <nx/vms/client/core/resource/screen_recording/audio_device_info.h>

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API AudioRecordingSettings: public nx::utils::property_storage::Storage
{
public:
    static QString kNoDevice;

    AudioRecordingSettings();

    Property<QString> primaryAudioDeviceName{this, "primaryAudioDevice"};
    Property<QString> secondaryAudioDeviceName{this, "secondaryAudioDevice"};

    QList<AudioDeviceInfo> availableDevices() const;

    AudioDeviceInfo primaryAudioDevice() const;
    AudioDeviceInfo secondaryAudioDevice() const;

    AudioDeviceInfo defaultPrimaryAudioDevice() const;
    AudioDeviceInfo defaultSecondaryAudioDevice() const;

private:
    AudioDeviceInfo getDeviceByName(const QString& name) const;

private:
    QList<AudioDeviceInfo> m_devices;
};

} // namespace nx::vms::client::core
