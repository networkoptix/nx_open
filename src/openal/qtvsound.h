/*
* qtvsound.h
*
*  Created on: Jun 2009
*      Author: ascii zhot
*/
#ifndef __QTVSOUND_H__
#define __QTVSOUND_H__

#include <QVector>

class QtvAudioDevice;
typedef struct ALCdevice_struct ALCdevice;

class QtvSound {
public:
	QtvSound(ALCdevice* device, uint numChannels, uint bitsPerSample, uint frequency, uint size);
	~QtvSound();

	bool isValid() const { return m_isValid; }

	uint playTimeElapsed() const;
	bool play(const void* data, uint size);
    void suspend();
    void resume();
    void clear();
    bool isFormatSupported();
private:
	uint bufferTime() const;
	bool setup();
	bool setFormat();
	uint bitRate() const;
	bool playImpl() const;

	static bool outError(int err, const char* strerr);
	static int checkOpenALErrorDebug(ALCdevice* device);

private:
	QVector<uint> m_buffers;
	uint m_source;
	uint m_format;
	uint m_numChannels;
	uint m_frequency;
	uint m_bitsPerSample;
	uint m_size;
	bool m_isValid;
	ALCdevice* m_device;
private:
    void internalClear();
public:
	static int checkOpenALError(ALCdevice* device);
};

#endif
