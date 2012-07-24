#ifndef audiostreamdisplay_h_1811
#define audiostreamdisplay_h_1811

#include "decoders/audio/audio_struct.h"
#include "openal/qtvsound.h"
#include "openal/qtvaudiodevice.h"
#include "core/datapacket/mediadatapacket.h"
#include "utils/common/aligned_data.h"

class CLAbstractAudioDecoder;
struct QnCompressedAudioData;

class CLAudioStreamDisplay : public QObject
{
	Q_OBJECT
public:
	CLAudioStreamDisplay(int buffMs, int prebufferMs);
	~CLAudioStreamDisplay();

    void putData(QnCompressedAudioDataPtr data, qint64 minTime);
    void enqueueData(QnCompressedAudioDataPtr data, qint64 minTime);
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

    void playCurrentBuffer();

    int getAudioBufferSize() const;
private:
    int msInQueue() const;

	static QnCodecAudioFormat downmix(QnByteArray& audio, QnCodecAudioFormat format);
	static QnCodecAudioFormat float2int16(QnByteArray& audio, QnCodecAudioFormat format);
	static QnCodecAudioFormat float2int32(QnByteArray& audio, QnCodecAudioFormat format);
    static QnCodecAudioFormat int32Toint16(QnByteArray& audio, QnCodecAudioFormat format);
	bool initFormatConvertRule(QnAudioFormat format);
private:
    QMutex m_guiSync;
	enum SampleConvertMethod {SampleConvert_None, SampleConvert_Float2Int32, SampleConvert_Float2Int16, SampleConvert_Int32ToInt16};

    QMap<CodecID, CLAbstractAudioDecoder*> m_decoder;

//	CLAbstractAudioDecoder* m_decoder[CL_VARIOUSE_DECODERS];

    int m_bufferMs;
    int m_prebufferMs;
    QnByteArray m_decodedAudioBuffer;
    bool m_tooFewDataDetected;
    bool m_isFormatSupported;
    QtvSound* m_audioSound;
    QQueue<QnCompressedAudioDataPtr> m_audioQueue;

    bool m_downmixing;    // do downmix.
    bool m_forceDownmix;  // force downmix, even if output device supports multichannel
    SampleConvertMethod m_sampleConvertMethod;
    bool m_isConvertMethodInitialized;
};

#endif //audiostreamdisplay_h_1811
