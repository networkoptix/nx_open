#ifndef audiostreamdisplay_h_1811
#define audiostreamdisplay_h_1811

#include <QMutex>
#include "data\mediadata.h"
#include <QAudio>
#include "..\src\multimedia\audio\qaudioformat.h"
#include "base\aligned_data.h"
#include "base\ringbuffer.h"


// display one video stream
// decodes the video and pass it to video window
class CLAbstractAudioDecoder;
struct CLCompressedAudioData;
class QAudioOutput;
class QIODevice;



class CLAudioStreamDisplay : public QObject
{
	Q_OBJECT
public:
	CLAudioStreamDisplay(int buff_ms);
	~CLAudioStreamDisplay();

	void putdata(CLCompressedAudioData* data);
private:

	void stopaudio();
	void recreatedevice(QAudioFormat format);

	unsigned int ms_from_size(const QAudioFormat& format, unsigned long bytes);
	unsigned int bytes_from_time(const QAudioFormat& format, unsigned long ms);

private:
	CLAbstractAudioDecoder* m_decoder[CL_VARIOUSE_DECODERS];
	int m_buff_ms;

	int m_can_adapt; // can not adapt device frequency very often;
	qreal m_freq_factor;

	CLAlignedData m_decodedaudio;

	CLRingBuffer* m_ringbuff;

	QAudioOutput* m_audioOutput;
	QIODevice* m_audiobuff; // not owned by this class 
};

#endif //audiostreamdisplay_h_1811