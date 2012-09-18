/*
* qtvaudioqnresource.h
*
*  Created on: Jun 2009
*      Author: ascii zhot
*/

#ifndef __QTVAUDIODEVICE_H__
#define __QTVAUDIODEVICE_H__

#include <QtCore/QList>
#include <QtCore/QObject>

#ifndef Q_OS_WIN
#include "utils/media/audioformat.h"
#else
#include <QAudioFormat>
#define QnAudioFormat QAudioFormat
#endif

typedef struct ALCdevice_struct ALCdevice;
typedef struct ALCcontext_struct ALCcontext;

class QnAudioFormat;
class QSettings;

class QtvSound;

class QtvAudioDevice: public QObject {
    Q_OBJECT
public:
    static QtvAudioDevice *instance();

    QtvAudioDevice(QObject *parent = NULL);
    ~QtvAudioDevice();

    QtvSound *addSound(const QnAudioFormat &format);
    void removeSound(QtvSound *soundObject);

    // in range 0..1
    float volume() const;
    void setVolume(float value);

    bool isMute() const;
    void setMute(bool mute);

    QString versionString() const;
    QString company() const;

signals:
    void volumeChanged();

private:
    ALCdevice *m_device;
    ALCcontext *m_context;
    float m_volume;
    QList<QtvSound *> m_sounds;
    QSettings *m_settings;
};

#endif // __QTVAUDIODEVICE_H__
