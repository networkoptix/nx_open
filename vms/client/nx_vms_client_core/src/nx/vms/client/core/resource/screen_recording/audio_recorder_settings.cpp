// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "audio_recorder_settings.h"

#include <QtCore/QDir>
#include <QtMultimedia/QAudioDeviceInfo>

#if defined(Q_OS_WIN)
    #include <nx/vms/client/core/resource/screen_recording/audio_device_info_win.h>
#endif // defined(Q_OS_WIN)

#include <nx/utils/app_info.h>
#include <nx/build_info.h>

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

AudioRecorderSettings::DeviceList fetchDevicesList()
{
    auto availableDevices = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);

    if (nx::build_info::isWindows())
    {
        // Under modern Windows systems there is an issue with duplicating entries returning by
        // QAudioDeviceInfo::availableDevices(), one from WASAPI, one from winMM, see QTBUG-75781.
        // Since they are actually the same devices provided by different interfaces, bad things
        // may occur when both are selected as input source (immediate crash on use attempt).

        // Device order will be kept.
        QStringList nameOrder;
        for (const auto& deviceInfo: availableDevices)
            nameOrder.append(deviceInfo.deviceName());
        nameOrder.removeDuplicates();

        std::sort(availableDevices.begin(), availableDevices.end(),
            [&nameOrder](const QAudioDeviceInfo& l, const QAudioDeviceInfo& r)
            {
                // First WASAPI one (1-2 channels usually), then winMM one (18 channels usually).
                // Qt 5.14 provides QAudioDeviceInfo::realm() getter, it should be used instead.
                if (l.deviceName() == r.deviceName())
                    return l.supportedChannelCounts() < r.supportedChannelCounts();
                return nameOrder.indexOf(l.deviceName()) < nameOrder.indexOf(r.deviceName());
            });

        const auto getDuplicatingWasapiDeviceItr =
            [&availableDevices]()
            {
                return std::adjacent_find(availableDevices.begin(), availableDevices.end(),
                    [](const QAudioDeviceInfo& l, const QAudioDeviceInfo& r)
                    {
                        return l.deviceName() == r.deviceName();
                    });
            };

        while (getDuplicatingWasapiDeviceItr() != availableDevices.end())
            availableDevices.erase(getDuplicatingWasapiDeviceItr());
    }

    AudioRecorderSettings::DeviceList devices;
    QMap<QString, int> countByName; //< #vbreus Is it still makes sense, or just was workaround?
    for (const auto& info: availableDevices)
    {
        const QString name = getFullDeviceName(info.deviceName());
        int& count = countByName[name];
        ++count;
        devices.append(AudioDeviceInfo(info, name, count));
    }

    return devices;
}

} // namespace

QString AudioRecorderSettings::kNoDevice = "<none>";

AudioRecorderSettings::AudioRecorderSettings(QObject* parent):
    QObject(parent),
    m_devices([&mutex = m_mutex](){ NX_MUTEX_LOCKER lock(&mutex); return fetchDevicesList(); })
{
    m_settings.beginGroup("videoRecording"); //< Don't rename, backward compatibility.
}

AudioRecorderSettings::~AudioRecorderSettings()
{
}

AudioDeviceInfo AudioRecorderSettings::getDeviceByName(const QString& name) const
{
    if (!name.isEmpty())
    {
        for (const AudioDeviceInfo& info: m_devices.get())
        {
            if (name == info.fullName())
                return info;
        }
    }

    return {};
}

AudioDeviceInfo AudioRecorderSettings::primaryAudioDevice() const
{
    const auto& name = primaryAudioDeviceName();
    if (name == kNoDevice)
        return {};

    const AudioDeviceInfo& info = getDeviceByName(name);
    if (!info.isNull())
        return info;

    return defaultPrimaryAudioDevice();
}

QString AudioRecorderSettings::primaryAudioDeviceName() const
{
    return m_settings.value("primaryAudioDevice").toString();
}

void AudioRecorderSettings::setPrimaryAudioDeviceName(const QString& audioDeviceName)
{
    m_settings.setValue("primaryAudioDevice", audioDeviceName);
}

AudioDeviceInfo AudioRecorderSettings::secondaryAudioDevice() const
{
    const auto& name = secondaryAudioDeviceName();
    if (name == kNoDevice)
        return {};

    const AudioDeviceInfo& device = getDeviceByName(name);
    if (!device.isNull())
        return device;

    return defaultSecondaryAudioDevice();
}

QString AudioRecorderSettings::secondaryAudioDeviceName() const
{
    return m_settings.value("secondaryAudioDevice").toString();
}

void AudioRecorderSettings::setSecondaryAudioDeviceName(const QString& audioDeviceName)
{
    m_settings.setValue("secondaryAudioDevice", audioDeviceName);
}

AudioDeviceInfo AudioRecorderSettings::defaultPrimaryAudioDevice() const
{
    const DeviceList& devices = availableDevices();

    // On windows it always returns non-existent "Default Audio Input Device".
    const QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();

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

AudioDeviceInfo AudioRecorderSettings::defaultSecondaryAudioDevice() const
{
    const QStringList checkList{"Rec. Playback", "Stereo mix", "What you hear"};

    for (const auto& name: checkList)
    {
        const AudioDeviceInfo& info = getDeviceByName(name);
        if (!info.isNull())
            return info;
    }

    return {};
}

QString AudioRecorderSettings::recordingFolder() const
{
    const auto dir = m_settings.value("recordingFolder").toString();
    if (!dir.isEmpty())
        return dir;

    return QDir::tempPath();
}

void AudioRecorderSettings::setRecordingFolder(const QString& folder)
{
    m_settings.setValue("recordingFolder", folder);
}

} // namespace nx::vms::client::core
