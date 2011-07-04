/*
* qtvsound.h
*
*  Created on: Jun 2009
*      Author: ascii zhot
*/
#ifndef __QTVSOUND_H__
#define __QTVSOUND_H__

#include <QVector>
#include <QAudioFormat>

class QtvAudioDevice;
typedef struct ALCdevice_struct ALCdevice;

class QtvSound {
public:
	QtvSound(ALCdevice* device, const QAudioFormat& audioFormat);
	~QtvSound();

	bool isValid() const { return m_isValid; }

	uint playTimeElapsed() const;
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
	bool playImpl() const;

	static bool outError(int err, const char* strerr);
	static int checkOpenALErrorDebug(ALCdevice* device);
    bool internalPlay(const void* data, uint size);
private:
	QAudioFormat m_audioFormat;
	QVector<uint> m_buffers;
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

#endif
