#include "audio_recorder_settings.h"

#include <QtCore/QDir>
#include <QtMultimedia/QAudioDeviceInfo>

#if defined(Q_OS_WIN)
#include <plugins/resource/desktop_win/win_audio_device_info.h>
#endif // defined(Q_OS_WIN)

#include <nx/utils/app_info.h>

namespace {

static const QString kNone = "<none>";

QString getFullDeviceName(const QString& shortName)
{
    #if defined(Q_OS_WIN)
        return QnWinAudioDeviceInfo(shortName).fullName();
    #else
        return shortName;
    #endif
}

QnAudioRecorderSettings::DeviceList fetchDevicesList()
{
    QnAudioRecorderSettings::DeviceList devices;

    QMap<QString, int> countByName;
    for (const auto& info: QAudioDeviceInfo::availableDevices(QAudio::AudioInput))
    {
        const QString name = getFullDeviceName(info.deviceName());
        int& count = countByName[name];
        ++count;
        devices.append(QnAudioDeviceInfo(info, name, count));
    }

    return devices;
}

} // namespace

QString QnAudioRecorderSettings::kNoDevice = "<none>";

QnAudioRecorderSettings::QnAudioRecorderSettings(QObject* parent):
    QObject(parent),
    m_mutex(new QnMutex),
    m_devices([](){ return fetchDevicesList(); }, m_mutex)
{
    m_settings.beginGroup("videoRecording"); //< Don't rename, backward compatibility.
}

QnAudioRecorderSettings::~QnAudioRecorderSettings()
{
}

QnAudioDeviceInfo QnAudioRecorderSettings::getDeviceByName(const QString& name) const
{
    if (!name.isEmpty())
    {
        for (const QnAudioDeviceInfo& info: m_devices.get())
        {
            if (name == info.fullName())
                return info;
        }
    }

    return {};
}

QnAudioDeviceInfo QnAudioRecorderSettings::primaryAudioDevice() const
{
    const auto& name = primaryAudioDeviceName();
    if (name == kNoDevice)
        return {};

    const QnAudioDeviceInfo& info = getDeviceByName(name);
    if (!info.isNull())
        return info;

    return defaultPrimaryAudioDevice();
}

QString QnAudioRecorderSettings::primaryAudioDeviceName() const
{
    return m_settings.value("primaryAudioDevice").toString();
}

void QnAudioRecorderSettings::setPrimaryAudioDeviceName(const QString& audioDeviceName)
{
    m_settings.setValue("primaryAudioDevice", audioDeviceName);
}

QnAudioDeviceInfo QnAudioRecorderSettings::secondaryAudioDevice() const
{
    const auto& name = secondaryAudioDeviceName();
    if (name == kNoDevice)
        return {};

    const QnAudioDeviceInfo& device = getDeviceByName(name);
    if (!device.isNull())
        return device;

    return defaultSecondaryAudioDevice();
}

QString QnAudioRecorderSettings::secondaryAudioDeviceName() const
{
    return m_settings.value("secondaryAudioDevice").toString();
}

void QnAudioRecorderSettings::setSecondaryAudioDeviceName(const QString& audioDeviceName)
{
    m_settings.setValue("secondaryAudioDevice", audioDeviceName);
}

QnAudioDeviceInfo QnAudioRecorderSettings::defaultPrimaryAudioDevice() const
{
    const DeviceList& devices = availableDevices();

    const QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();
    if (info.isNull() && !devices.isEmpty())
        return devices.first();

    const auto it = std::find_if(devices.begin(), devices.end(),
        [&info](const QnAudioDeviceInfo& item) { return info == item; });

    if (it != devices.end())
        return *it;

    return {};
}

QnAudioDeviceInfo QnAudioRecorderSettings::defaultSecondaryAudioDevice() const
{
    const QStringList checkList{"Rec. Playback", "Stereo mix", "What you hear"};

    for (const auto& name: checkList)
    {
        const QnAudioDeviceInfo& info = getDeviceByName(name);
        if (!info.isNull())
            return info;
    }

    return {};
}

QString QnAudioRecorderSettings::recordingFolder() const
{
    const auto dir = m_settings.value("recordingFolder").toString();
    if (!dir.isEmpty())
        return dir;

    return QDir::tempPath();
}

void QnAudioRecorderSettings::setRecordingFolder(const QString& folder)
{
    m_settings.setValue("recordingFolder", folder);
}
