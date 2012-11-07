#include "qnaudio_device_info.h"
#include "video_recorder_settings.h"

QnAudioDeviceInfo::QnAudioDeviceInfo(): QAudioDeviceInfo()
{
}

QnAudioDeviceInfo::QnAudioDeviceInfo(const QAudioDeviceInfo& other, const QString& fullName):
    QAudioDeviceInfo(other),
    m_fullName(fullName)
{

}


void QnAudioDeviceInfo::splitFullName(QString& name, int& devNum) const
{
    QnVideoRecorderSettings::splitFullName(m_fullName, name, devNum);
}
