#ifndef QN_WIN_AUDIO_EXTEND_INFO_H
#define QN_WIN_AUDIO_EXTEND_INFO_H

#include <QtCore/QtGlobal>

#ifdef Q_OS_WIN

#include <QtCore/QString>
#include <QtCore/QScopedPointer>
#include <QtGui/QPixmap>

class QnWinAudioDeviceInfoPrivate;

class QnWinAudioDeviceInfo {
public:
    QnWinAudioDeviceInfo(const QString &deviceName);
    virtual ~QnWinAudioDeviceInfo();

    QString fullName() const;
    bool isMicrophone() const;
    QPixmap deviceIcon() const;

private:
    QScopedPointer<QnWinAudioDeviceInfoPrivate> d;
};

#endif // Q_OS_WIN

#endif // QN_WIN_AUDIO_EXTEND_INFO_H
