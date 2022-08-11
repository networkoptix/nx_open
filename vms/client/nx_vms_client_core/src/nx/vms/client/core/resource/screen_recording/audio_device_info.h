// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtMultimedia/QAudioDeviceInfo>

namespace nx::vms::client::core {

class AudioDeviceInfo: public QAudioDeviceInfo
{
public:
    AudioDeviceInfo() = default;
    AudioDeviceInfo(const QAudioDeviceInfo& info, const QString& name, int deviceNumber = 1):
        QAudioDeviceInfo(info),
        m_name(name),
        m_deviceNumber(deviceNumber)
    {
    }

    QString name() const { return m_name; }
    int deviceNumber() const { return m_deviceNumber; }

    QString fullName() const
    {
        if (m_name.isEmpty())
            return deviceName();

        if (m_deviceNumber <= 1)
            return m_name;

        return m_name + QStringLiteral(" (%1)").arg(m_deviceNumber);
    }

private:
    QString m_name;
    int m_deviceNumber = 1;
};

} // namespace nx::vms::client::core
