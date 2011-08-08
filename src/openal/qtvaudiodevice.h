/*
* qtvaudiodevice.h
*
*  Created on: Jun 2009
*      Author: ascii zhot
*/

#ifndef __QTVAUDIODEVICE_H__
#define __QTVAUDIODEVICE_H__

#include <QList>
#include <QMutex>
#include <QSettings>

typedef struct ALCdevice_struct ALCdevice;
typedef struct ALCcontext_struct ALCcontext;
class QtvSound;

class QtvAudioDevice {
public:

    QtvAudioDevice();
    static QtvAudioDevice& instance();
    ~QtvAudioDevice();
    void release();

    QtvSound* addSound(const QAudioFormat& format);
    void removeSound(QtvSound* soundObject);

    void setVolume(float value); // in range 0..1
    float getVolume() const;

private:
    QList<QtvSound*> m_sounds;
    ALCcontext* m_context;
    ALCdevice* m_device;
    QSettings m_settings;
};

#endif
