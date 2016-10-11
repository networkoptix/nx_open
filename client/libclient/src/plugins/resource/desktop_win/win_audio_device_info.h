#pragma once

#include <QtCore/QScopedPointer>
#include <QtGui/QPixmap>

class QnWinAudioDeviceInfoPrivate;

class QnWinAudioDeviceInfo
{
public:
    QnWinAudioDeviceInfo(const QString& deviceName);
    virtual ~QnWinAudioDeviceInfo();

    QString fullName() const;
    bool isMicrophone() const;
    QPixmap deviceIcon() const;

private:
    QScopedPointer<QnWinAudioDeviceInfoPrivate> d;
};
