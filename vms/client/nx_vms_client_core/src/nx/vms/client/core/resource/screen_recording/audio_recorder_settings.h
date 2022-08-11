// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QSettings>

#include <nx/utils/value_cache.h>

#include "audio_device_info.h"

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API AudioRecorderSettings: public QObject
{
public:
    using DeviceList = QList<AudioDeviceInfo>;

    static QString kNoDevice;

    explicit AudioRecorderSettings(QObject* parent = nullptr);
    virtual ~AudioRecorderSettings() override;

    DeviceList availableDevices() const { return m_devices.get(); }

    AudioDeviceInfo primaryAudioDevice() const;

    QString primaryAudioDeviceName() const;
    void setPrimaryAudioDeviceName(const QString& name);

    AudioDeviceInfo secondaryAudioDevice() const;

    QString secondaryAudioDeviceName() const;
    void setSecondaryAudioDeviceName(const QString& name);

    AudioDeviceInfo defaultPrimaryAudioDevice() const;
    AudioDeviceInfo defaultSecondaryAudioDevice() const;

    QString recordingFolder() const;
    void setRecordingFolder(const QString& folder);

private:
    AudioDeviceInfo getDeviceByName(const QString& name) const;

private:
    nx::Mutex m_mutex;
    nx::utils::CachedValue<DeviceList> m_devices;
    QSettings m_settings;
};

} // namespace nx::vms::client::core
