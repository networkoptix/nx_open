/*
* qtvaudioqnresource.h
*
*  Created on: Jun 2009
*      Author: ascii zhot
*/

#ifndef __QTVAUDIODEVICE_H__
#define __QTVAUDIODEVICE_H__

#include <QtCore/QList>

typedef struct ALCdevice_struct ALCdevice;
typedef struct ALCcontext_struct ALCcontext;

class QAudioFormat;
class QSettings;

class QtvSound;

class QtvAudioDevice
{
public:
    static QtvAudioDevice *instance();

    QtvAudioDevice();
    ~QtvAudioDevice();

    QtvSound *addSound(const QAudioFormat &format);
    void removeSound(QtvSound *soundObject);

    // in range 0..1
    float volume() const;
    void setVolume(float value);

    bool isMute() const;
    void setMute(bool mute);

private:
    ALCdevice *m_device;
    ALCcontext *m_context;
    float m_volume;
    QList<QtvSound *> m_sounds;
    QSettings *m_settings;
};

#endif // __QTVAUDIODEVICE_H__
