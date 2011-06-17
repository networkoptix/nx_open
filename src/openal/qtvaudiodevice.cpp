#include "stdafx.h"
#include "qtvaudiodevice.h"

#include "qtvsound.h"

#ifdef Q_OS_MAC
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <al.h>
#include <alc.h>
#endif

#define OPENAL_STATIC

//#include <algorithm>
#ifdef OPENAL_STATIC
extern "C" {
void alc_init(void);
void alc_deinit(void);
#pragma comment(lib, "winmm.lib")
//"msvcrt.lib"
}
#endif

QtvAudioDevice* QtvAudioDevice::m_qtvAudioDevice = 0;


QtvAudioDevice::QtvAudioDevice() 
{
#ifdef OPENAL_STATIC
    alc_init();
#endif
	qDebug("%s", "OpenAL init");
	m_device = alcOpenDevice(NULL);
	if (m_device == 0)
	{
		qWarning() << "Sound card not found or sound error";
		return;
	}
	//Q_ASSERT(m_device && "NULL Pointer");
	if (!m_device) {
		qDebug("%s", "OpenAL error open Device");
//		return false;
	}
	m_context = alcCreateContext(m_device, NULL);
	Q_ASSERT(m_context && "NULL Pointer");
	if (!m_context) {
		qDebug("%s", "OpenAL error create context");
//		return false;
	}
	alcMakeContextCurrent(m_context);
	QtvSound::checkOpenALError(m_device);
	// get version
	int majorVersion = 0;
	int minorVersion = 0;
	alcGetIntegerv(m_device, ALC_MAJOR_VERSION, 1, &majorVersion);
	alcGetIntegerv(m_device, ALC_MINOR_VERSION, 1, &minorVersion);
	// Check what device and version we are using
	const char* name = alcGetString(m_device, ALC_DEVICE_SPECIFIER);
	Q_ASSERT(name && "NULL Pointer");
	qDebug("%s%s%s%d%s%d", "Opened, ", name ? name : "",  " spec version ", majorVersion, ".", minorVersion);
	qDebug("%s%s", "Company: ", static_cast<const char *>(alGetString(AL_VENDOR)));
	qDebug("%s%s", "Device type: ", static_cast<const char *>(alGetString(AL_RENDERER)));
	qDebug("%s%s", "OpenAL extensions: ", static_cast<const char *>(alGetString(AL_EXTENSIONS)));
	qDebug("%s", "OpenAL init ok");
//	return true;

}

QtvAudioDevice::~QtvAudioDevice()
{
#ifdef OPENAL_STATIC
    alc_deinit();
#endif
}

void QtvAudioDevice::removeSound(QtvSound* soundObject)
{
	if (!soundObject) {
		return;
	}
	bool isRemoved = m_sounds.removeOne(soundObject);
	Q_ASSERT(isRemoved);
	delete soundObject;
}


QtvSound* QtvAudioDevice::addSound(uint numChannels, uint bitsPerSample, uint frequency, uint size)
{
	if (m_device == 0) {
		qWarning() << "Addsound command is ignored because sound card not found";
		return 0;
	}
	//Q_ASSERT(m_context && m_device);
	if (!m_context || !m_device) {
		return 0;
	}

	QtvSound* result = new QtvSound(m_device, numChannels, bitsPerSample, frequency, size);
	if (!result->isValid()) {
		delete result;
		return 0;
	}
	m_sounds.push_back(result);
	return result;
}



void QtvAudioDevice::release()
{
	while (!m_sounds.isEmpty()) {
		delete m_sounds.takeFirst();
	}

	// Disable context
	alcMakeContextCurrent(NULL);
	if (m_context) {
		// Release context(s)
		alcDestroyContext(m_context);
		m_context = NULL;
	}
	if (m_device) {
		// Close device
		alcCloseDevice(m_device);
		m_device = NULL;
	}
}

QtvAudioDevice& QtvAudioDevice::instance()
{
	if (!m_qtvAudioDevice) {
		m_qtvAudioDevice = new QtvAudioDevice();
	}
	return *m_qtvAudioDevice;
}
