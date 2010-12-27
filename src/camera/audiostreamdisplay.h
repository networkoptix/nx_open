#ifndef audiostreamdisplay_h_1811
#define audiostreamdisplay_h_1811

#include <QMutex>
#include "data\mediadata.h"
#include <QAudio>
#include "..\src\multimedia\audio\qaudioformat.h"


// display one video stream
// decodes the video and pass it to video window
class CLAbstractAudioDecoder;
struct CLCompressedAudioData;
class QAudioOutput;
class QIODevice;


class CLRingBuffer : public QIODevice
{
	Q_OBJECT
public:
	CLRingBuffer( int capacity, QObject* parent);
	~CLRingBuffer();

	qint64 readData(char *data, qint64 maxlen);
	qint64 writeData(const char *data, qint64 len);
	qint64 bytesAvailable() const;

	qint64 avalable_to_write() const;

private:
	char *m_buff;
	int m_capacity;

	char* m_pw;
	char* m_pr;

	mutable QMutex m_mtx;

};



class CLAudioStreamDisplay : public QObject
{
	Q_OBJECT
public:
	CLAudioStreamDisplay(int buff_ms);
	~CLAudioStreamDisplay();

	void putdata(CLCompressedAudioData* data);
private:

	void stopaudio();

private slots:
	void stateChanged(QAudio::State state);


private:
	CLAbstractAudioDecoder* m_decoder[CL_VARIOUSE_DECODERS];
	int m_recommended_buff_ms;
	unsigned char* m_decodedaudio;
	CLRingBuffer m_ringbuff;

	QAudioOutput* m_audioOutput;
	QIODevice* m_audiobuff; // not owned by this class 
};

#endif //audiostreamdisplay_h_1811