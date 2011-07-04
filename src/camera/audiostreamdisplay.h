#ifndef audiostreamdisplay_h_1811
#define audiostreamdisplay_h_1811

#include "data/mediadata.h"
#include "base/aligned_data.h"
#include "decoders/audio/audio_struct.h"

#include "openal/qtvsound.h"
#include "openal/qtvaudiodevice.h"

class CLAbstractAudioDecoder;
struct CLCompressedAudioData;
class CLAudioDevice;

class CLAudioStreamDisplay : public QObject
{
	Q_OBJECT
public:
	CLAudioStreamDisplay(int buffMs);
	~CLAudioStreamDisplay();

	void putData(CLCompressedAudioData* data);
    void enqueueData(CLCompressedAudioData* data);
	void suspend();

	void resume();

	// removes all data from audio buffers
	void clearAudioBuffer();

    // clears only device buff, not packets queue
    void clearDeviceBuffer();

	// how many ms is buffered in audio buffers at this moment(!)
	int msInBuffer() const;

	// returns true if audio is not playing, just accumulating 
	bool isBuffering() const;

    // returns false if format is not supported 
    bool isFormatSupported() const;
	
	// forcing downmixing, even if output device supports multichannel output
	void setForceDownmix(bool value);

	bool isDownmixForced() const { return m_forceDownmix; }
private:
    // returns amount of ms which buffered before start playing 
    int playAfterMs() const;
    int msInQueue() const;

	static void downmix(CLAudioData& audio);
	static void float2int16(CLAudioData& audio);
	static void float2int32(CLAudioData& audio);
	static void int32Toint16(CLAudioData& audio);
	bool initFormatConvertRule(QAudioFormat format);
private:
	enum SampleConvertMethod {SampleConvert_None, SampleConvert_Float2Int32, SampleConvert_Float2Int16, SampleConvert_Int32ToInt16};

    QMap<CodecID, CLAbstractAudioDecoder*> m_decoder;

//	CLAbstractAudioDecoder* m_decoder[CL_VARIOUSE_DECODERS];

	int m_bufferMs;
	CLAlignedData m_decodedaudio;
	bool m_tooFewDataDetected;
	bool m_isFormatSupported;
    QtvSound* m_audioSound;
    QQueue<CLCompressedAudioData*> m_audioQueue;

	bool m_downmixing;    // do downmix. 
	bool m_forceDownmix;  // force downmix, even if output device supports multichannel
	SampleConvertMethod m_sampleConvertMethod;
	bool m_isConvertMethodInitialized;
};

#endif //audiostreamdisplay_h_1811
