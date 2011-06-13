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

typedef struct ALCdevice_struct ALCdevice;
typedef struct ALCcontext_struct ALCcontext;
class QtvSound;

class QtvAudioDevice {
public:
	static QtvAudioDevice& instance();
	~QtvAudioDevice();
	void release();

	QtvSound* addSound(uint numChannels, uint bitsPerSample, uint frequency, uint size);
	void removeSound(QtvSound* soundObject);

private:
	QtvAudioDevice();

private:
	static QtvAudioDevice* m_qtvAudioDevice;
	QList<QtvSound*> m_sounds;
	ALCcontext* m_context;
	ALCdevice* m_device;	
};

#endif
