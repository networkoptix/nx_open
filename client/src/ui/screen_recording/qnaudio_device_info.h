#ifndef __QN_AUDIO_DEVICE_INFO_H__
#define __QN_AUDIO_DEVICE_INFO_H__

#include "qaudiodeviceinfo.h"

class QnAudioDeviceInfo: public QAudioDeviceInfo
{
public:
    QnAudioDeviceInfo();
    QnAudioDeviceInfo(const QAudioDeviceInfo& other, const QString& fullName);

    QString fullName() const { return m_fullName.isEmpty() ? deviceName() : m_fullName; }
    void splitFullName(QString& name, int& devNum) const;
private:
    QString m_fullName;
};

#endif // __QN_AUDIO_DEVICE_INFO_H__
