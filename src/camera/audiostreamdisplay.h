#ifndef audiostreamdisplay_h_1811
#define audiostreamdisplay_h_1811

#include "data\mediadata.h"
#include "..\src\multimedia\audio\qaudioformat.h"
#include "base\aligned_data.h"
#include "decoders\audio\audio_struct.h"


// display one video stream
// decodes the video and pass it to video window
class CLAbstractAudioDecoder;
struct CLCompressedAudioData;
class CLAudioDevice;


class CLAudioStreamDisplay : public QObject
{
	Q_OBJECT
public:
	CLAudioStreamDisplay(int buff_ms);
	~CLAudioStreamDisplay();

	void putdata(CLCompressedAudioData* data);

    void enqueData(CLCompressedAudioData* data);

	void suspend();

	void resume();

	// removes all data from audio buffers
	void clearAudioBuff();

    // clears only device buff, not packets queue
    void clearDeviceBuff();

	// how many ms is buffered in audio buffers at this moment(!)
	int msInBuffer() const;

	// returns true if audio is not playing, just accumulating 
	bool isBuffring() const;

private:

    // returns amount of ms which buffered before start playing 
    int playAfterMs() const;
    int msInQueue() const;


	static void down_mix(CLAudioData& audio);
	static void float2int(CLAudioData& audio);
private:

	CLAbstractAudioDecoder* m_decoder[CL_VARIOUSE_DECODERS];

	int m_buff_ms;
	CLAlignedData m_decodedaudio;
	bool m_downmixing;
	bool m_too_few_data_detected;
    CLAudioDevice* m_audiodevice;

    QQueue<CLCompressedAudioData*> m_audioQueue;

    
};

#endif //audiostreamdisplay_h_1811