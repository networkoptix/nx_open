// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audio_recording_settings.h"

#include <QtCore/QStandardPaths>
#include <QtMultimedia/QAudioDevice>
#include <QtMultimedia/QMediaDevices>

#include <nx/build_info.h>
#include <nx/utils/property_storage/filesystem_backend.h>

#if defined(Q_OS_WIN)
#include <nx/vms/client/core/resource/screen_recording/audio_device_info_win.h>
#endif // defined(Q_OS_WIN)

namespace nx::vms::client::core {

namespace {

static const QString kNone = "<none>";

QString getFullDeviceName(const QString& shortName)
{
    #if defined(Q_OS_WIN)
        return WinAudioDeviceInfo(shortName).fullName();
    #else
        return shortName;
    #endif
}

QList<AudioDeviceInfo> fetchDevicesList()
{
    auto availableDevices = QMediaDevices::audioInputs();

    if (nx::build_info::isWindows())
    {
        // Under modern Windows systems there is an issue with duplicating entries returning by
        // QAudioDeviceInfo::availableDevices(), one from WASAPI, one from winMM, see QTBUG-75781.
        // Since they are actually the same devices provided by different interfaces, bad things
        // may occur when both are selected as input source (immediate crash on use attempt).
        // Suggested workaround with realm() method can't be used in Qt 6 - no such method exists.

        // Device order will be kept.
        QStringList nameOrder;
        for (const auto& deviceInfo: availableDevices)
            nameOrder.append(deviceInfo.description());
        nameOrder.removeDuplicates();

        std::sort(availableDevices.begin(), availableDevices.end(),
            [&nameOrder](const QAudioDevice& l, const QAudioDevice& r)
            {
                // First WASAPI one (1-2 channels usually), then winMM one (18 channels usually).
                if (l.description() == r.description())
                    return l.maximumChannelCount() < r.maximumChannelCount();
                return nameOrder.indexOf(l.description()) < nameOrder.indexOf(r.description());
            });

        const auto getDuplicatingWasapiDeviceItr =
            [&availableDevices]()
            {
                return std::adjacent_find(availableDevices.begin(), availableDevices.end(),
                    [](const QAudioDevice& l, const QAudioDevice& r)
                    {
                        return l.description() == r.description();
                    });
            };

        while (getDuplicatingWasapiDeviceItr() != availableDevices.end())
            availableDevices.erase(getDuplicatingWasapiDeviceItr());
    }

    QList<AudioDeviceInfo> devices;
    QMap<QString, int> countByName; //< #vbreus Is it still makes sense, or just was workaround?
    for (const auto& info: availableDevices)
    {
        const QString name = getFullDeviceName(info.description());
        int& count = countByName[name];
        ++count;
        devices.append(AudioDeviceInfo(info, name, count));
    }

    return devices;
}

} // namespace

QString AudioRecordingSettings::kNoDevice = "<none>";

AudioRecordingSettings::AudioRecordingSettings():
    Storage(new nx::utils::property_storage::FileSystemBackend(
        QStandardPaths::standardLocations(QStandardPaths::AppLocalDataLocation).first()
            + "/settings/screen_recording")),
    m_devices(fetchDevicesList())
{
}

QList<AudioDeviceInfo> AudioRecordingSettings::availableDevices() const
{
    return m_devices;
}

AudioDeviceInfo AudioRecordingSettings::primaryAudioDevice() const
{
    const auto name = primaryAudioDeviceName();
    if (name == kNoDevice)
        return {};

    const AudioDeviceInfo device = getDeviceByName(name);
    if (!device.isNull())
        return device;

    return defaultPrimaryAudioDevice();
}

AudioDeviceInfo AudioRecordingSettings::secondaryAudioDevice() const
{
    const auto name = secondaryAudioDeviceName();
    if (name == kNoDevice)
        return {};

    const AudioDeviceInfo device = getDeviceByName(name);
    if (!device.isNull())
        return device;

    return defaultSecondaryAudioDevice();
}

AudioDeviceInfo AudioRecordingSettings::defaultPrimaryAudioDevice() const
{
    const auto devices = availableDevices();

    // On Windows it always returns non-existent "Default Audio Input Device".
    const QAudioDevice info = QMediaDevices::defaultAudioInput();

    if (!info.isNull())
    {
        const auto it = std::find_if(devices.begin(), devices.end(),
            [&info](const AudioDeviceInfo& item) { return info == item; });

        if (it != devices.end())
            return *it;
    }

    if (!devices.isEmpty())
        return devices.first();

    return {};
}

AudioDeviceInfo AudioRecordingSettings::defaultSecondaryAudioDevice() const
{
    const QStringList checkList{"Rec. Playback", "Stereo mix", "What you hear"};

    for (const auto& name: checkList)
    {
        const AudioDeviceInfo info = getDeviceByName(name);
        if (!info.isNull())
            return info;
    }

    return {};
}

AudioDeviceInfo AudioRecordingSettings::getDeviceByName(const QString& name) const
{
    if (!name.isEmpty())
    {
        for (const AudioDeviceInfo& info: m_devices)
        {
            if (name == info.fullName())
                return info;
        }
    }

    return {};
}

} // namespace nx::vms::client::core
