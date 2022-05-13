// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QSettings>

#include <nx/utils/value_cache.h>

#include "audio_device_info.h"

class NX_VMS_CLIENT_CORE_API QnAudioRecorderSettings: public QObject
{
public:
    using DeviceList = QList<QnAudioDeviceInfo>;

    static QString kNoDevice;

    explicit QnAudioRecorderSettings(QObject* parent = nullptr);
    virtual ~QnAudioRecorderSettings() override;

    DeviceList availableDevices() const { return m_devices.get(); }

    QnAudioDeviceInfo primaryAudioDevice() const;

    QString primaryAudioDeviceName() const;
    void setPrimaryAudioDeviceName(const QString& name);

    QnAudioDeviceInfo secondaryAudioDevice() const;

    QString secondaryAudioDeviceName() const;
    void setSecondaryAudioDeviceName(const QString& name);

    QnAudioDeviceInfo defaultPrimaryAudioDevice() const;
    QnAudioDeviceInfo defaultSecondaryAudioDevice() const;

    QString recordingFolder() const;
    void setRecordingFolder(const QString& folder);

private:
    QnAudioDeviceInfo getDeviceByName(const QString& name) const;

private:
    nx::Mutex m_mutex;
    nx::utils::CachedValue<DeviceList> m_devices;
    QSettings m_settings;
};
