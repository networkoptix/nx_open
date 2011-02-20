#ifndef audiostreamdisplay_h_1811
#define audiostreamdisplay_h_1811

#include "data\mediadata.h"
#include "..\src\multimedia\audio\qaudioformat.h"
#include "base\aligned_data.h"
#include "base\ringbuffer.h"
#include "decoders\audio\audio_struct.h"


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
	CLAudioStreamDisplay(int buff_ms, int buffAccMs);
	~CLAudioStreamDisplay();

	void putdata(CLCompressedAudioData* data);
	int suspend();
	void resume();

	int msInAudio();

	void clear();

	bool is_initialized() const;
	int get_buf_size_ms() const { return m_buff_ms; }

	int msBuffered() { return ms_from_size(m_audioOutput->format(), m_ringbuff->bytesAvailable()) + msInAudio(); }

	QAudioOutput* m_audioOutput;
	CLRingBuffer* m_ringbuff;

private:

	void stopaudio();
	void recreatedevice(QAudioFormat format);

	unsigned int ms_from_size(const QAudioFormat& format, unsigned long bytes);
	unsigned int bytes_from_time(const QAudioFormat& format, unsigned long ms);

	static void down_mix(CLAudioData& audio);
	static void float2int(CLAudioData& audio);
private:
	CLAbstractAudioDecoder* m_decoder[CL_VARIOUSE_DECODERS];
	int m_buff_ms;

	int m_buffAccMs;
	int m_buffAccBytes;

	int m_can_adapt; // can not adapt device frequency very often;
	qreal m_freq_factor;

	CLAlignedData m_decodedaudio;

	QIODevice* m_audiobuff; // not owned by this class 

	bool m_downmixing;

	bool m_tooFewSet;

	bool m_too_few_data_detected;
};

#endif //audiostreamdisplay_h_1811