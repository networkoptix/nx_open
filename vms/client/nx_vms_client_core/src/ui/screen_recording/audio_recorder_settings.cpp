#include "audio_recorder_settings.h"

#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtMultimedia/QAudioDeviceInfo>

#if defined(Q_OS_WIN)
#include <plugins/resource/desktop_win/win_audio_device_info.h>
#endif // Q_OS_WIN

#include <utils/common/warnings.h>

namespace {

const QRegExp devNumberExpr(QLatin1String(" \\([0-9]+\\)$"));

} // namespace

QnAudioRecorderSettings::QnAudioRecorderSettings(QObject *parent):
    QObject(parent)
{
    settings.beginGroup(QLatin1String("videoRecording")); //< Don't rename, backward compatibility.
}

QnAudioRecorderSettings::~QnAudioRecorderSettings()
{
    settings.endGroup();
}

QString QnAudioRecorderSettings::getFullDeviceName(const QString& shortName)
{
#ifdef Q_OS_WIN
    return QnWinAudioDeviceInfo(shortName).fullName();
#else
    return shortName;
#endif
}

QStringList QnAudioRecorderSettings::availableDeviceNames(QAudio::Mode mode)
{
    QMap<QString, int> devices;
    for (const auto& info: QAudioDeviceInfo::availableDevices(mode))
    {
        QString devName = getFullDeviceName(info.deviceName());
        QMap<QString, int>::iterator existingDevice = devices.find(devName);
        if (existingDevice == devices.cend())
            devices.insert(devName, 1);
        else
            existingDevice.value()++;
    }

    QStringList result;
    for (auto itr = devices.cbegin(); itr != devices.cend(); ++itr)
    {
        result << itr.key();
        for (int i = 2; i <= itr.value(); ++i)
            result << (itr.key() + QString(QLatin1String(" (%1)")).arg(i));
    }

    return result;
}

void QnAudioRecorderSettings::splitFullName(const QString& name, QString& shortName, int& index)
{
    shortName = name;
    int sameNamePos = name.lastIndexOf(devNumberExpr);
    index = 1;
    if (sameNamePos > 0)
    {
        index = name.mid(sameNamePos +2 , name.lastIndexOf(QLatin1String(")")) - sameNamePos - 2)
            .toInt();
        shortName = name.left(sameNamePos);
    }
}

QnAudioDeviceInfo QnAudioRecorderSettings::getDeviceByName(const QString& _name, QAudio::Mode mode,
    bool* isDefault) const
{
    QString name;
    int devNum = 1;
    splitFullName(_name, name, devNum);

    if (isDefault)
        *isDefault = false;
    if (name == QObject::tr("None"))
        return QnAudioDeviceInfo();

    foreach (const QAudioDeviceInfo &info, QAudioDeviceInfo::availableDevices(mode))
    {
        if (name.startsWith(info.deviceName()))
        {
            if (--devNum == 0)
                return QnAudioDeviceInfo(info, _name);
        }
    }

    if (isDefault)
        *isDefault = true;

    return QnAudioDeviceInfo(QAudioDeviceInfo::defaultInputDevice(), QString());
}

QnAudioDeviceInfo QnAudioRecorderSettings::primaryAudioDevice() const
{
    return getDeviceByName(settings.value(QLatin1String("primaryAudioDevice")).toString(),
        QAudio::AudioInput);
}

QString QnAudioRecorderSettings::primaryAudioDeviceName() const
{
    return settings.value(QLatin1String("primaryAudioDevice")).toString();
}

void QnAudioRecorderSettings::setPrimaryAudioDeviceByName(const QString& audioDeviceName)
{
    settings.setValue(QLatin1String("primaryAudioDevice"), audioDeviceName);
}

QnAudioDeviceInfo QnAudioRecorderSettings::secondaryAudioDevice() const
{
    QnAudioDeviceInfo result;
    bool isDefault = true;
    if (isDefault)
        result = getDeviceByName(QLatin1String("Rec. Playback"), QAudio::AudioInput, &isDefault);
    if (isDefault)
        result = getDeviceByName(QLatin1String("Stereo mix"), QAudio::AudioInput, &isDefault);
    if (isDefault)
        result = getDeviceByName(QLatin1String("What you hear"), QAudio::AudioInput, &isDefault);
    if (!isDefault)
        return result;

    return getDeviceByName(settings.value(QLatin1String("secondaryAudioDevice")).toString(),
        QAudio::AudioInput);
}

QString QnAudioRecorderSettings::secondaryAudioDeviceName() const
{
    return settings.value(QLatin1String("secondaryAudioDevice")).toString();
}

void QnAudioRecorderSettings::setSecondaryAudioDeviceByName(const QString& audioDeviceName)
{
    settings.setValue(QLatin1String("secondaryAudioDevice"), audioDeviceName);
}

QString QnAudioRecorderSettings::recordingFolder() const
{
    if (!settings.contains(QLatin1String("recordingFolder")))
        return QDir::tempPath();
    return settings.value(QLatin1String("recordingFolder")).toString();
}

void QnAudioRecorderSettings::setRecordingFolder(QString folder)
{
    settings.setValue(QLatin1String("recordingFolder"), folder);
}
