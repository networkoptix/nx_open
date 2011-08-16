/*
* qtvsound.h
*
*  Created on: Jun 2009
*      Author: ascii zhot
*/

#ifndef __QTVSOUND_H__
#define __QTVSOUND_H__

#include <QtCore/QMutex>
#include <QtCore/QVector>

#include <QtMultimedia/QAudioFormat>

#ifdef _WIN32
#  define OPENAL_WIN32_ONLY
#endif

class QtvAudioDevice;
typedef struct ALCdevice_struct ALCdevice;

class QtvSound
{
public:
    QtvSound(ALCdevice* device, const QAudioFormat& audioFormat);
    ~QtvSound();

    bool isValid() const { return m_isValid; }

    uint playTimeElapsed();
    void setVolumeLevel(float value); // in range 0..1
    float getVolumeLevel() const;

    bool play(const quint8* data, uint size);
    void suspend();
    void resume();
    void clear();
    static bool isFormatSupported(const QAudioFormat& format);

private:
    uint bufferTime() const;
    bool setup();
    static int getFormat(const QAudioFormat& audioFormat);
    uint bitRate() const;
    bool playImpl();

    static bool outError(int err, const char* strerr);
    static int checkOpenALErrorDebug(ALCdevice* device);
    bool internalPlay(const void* data, uint size);
    void clearBuffers(bool clearAll);

private:
    mutable QMutex m_mtx;
    QAudioFormat m_audioFormat;

#ifdef OPENAL_WIN32_ONLY
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
    ALCdevice* m_device;
    quint8* m_proxyBuffer;
    int m_proxyBufferLen;
    bool m_deinitialized;

private:
    void internalClear();

public:
    static int checkOpenALError(ALCdevice* device);
};

#endif // __QTVSOUND_H__
