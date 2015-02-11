/*
* qtvsound.h
*
*  Created on: Jun 2009
*      Author: ascii zhot
*/

#ifndef __QTVSOUND_H__
#define __QTVSOUND_H__

#include <utils/common/mutex.h>
#include <QtCore/QVector>

#ifndef Q_OS_WIN
#include "utils/media/audioformat.h"
#else
#include <QtMultimedia/QAudioFormat>
#define QnAudioFormat QAudioFormat
#endif

#define OPENAL_NEW_ALGORITHM

class QtvAudioDevice;
typedef struct ALCdevice_struct ALCdevice;

class QtvSound
{
public:
    QtvSound(ALCdevice *device, const QnAudioFormat &audioFormat);
    ~QtvSound();

    bool isValid() const { return m_isValid; }

    // in range 0..1
    float volumeLevel() const;
    void setVolumeLevel(float volumeLevel);

    uint playTimeElapsed();

    bool play(const quint8 *data, uint size);
    void suspend();
    void resume();
    void clear();

    static bool isFormatSupported(const QnAudioFormat &format);

private:
    uint bufferTime() const;
    bool setup();
    static int getFormat(const QnAudioFormat &audioFormat);
    uint bitRate() const;
    bool playImpl();

    static bool outError(int err, const char *strerr);
    static int checkOpenALErrorDebug(ALCdevice *device);
    bool internalPlay(const void *data, uint size);
    void clearBuffers(bool clearAll);

private:
    mutable QnMutex m_mtx;
    QnAudioFormat m_audioFormat;

#ifdef OPENAL_NEW_ALGORITHM
    uint m_tmpBuffer[1024];
#else
    QVector<uint> m_buffers;
#endif

    uint m_source;
    uint m_format;
    uint m_numChannels;
    uint m_frequency;
    uint m_bitsPerSample;
    uint m_size;
    bool m_isValid;
    ALCdevice *m_device;
    quint8 *m_proxyBuffer;
    int m_proxyBufferLen;
    bool m_deinitialized;

private:
    void internalClear();

public:
    static int checkOpenALError(ALCdevice *device);
};

#endif // __QTVSOUND_H__
