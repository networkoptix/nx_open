#ifndef QN_AUDIO_STREAM_DISPLAY
#define QN_AUDIO_STREAM_DISPLAY

#include <utils/common/mutex.h>

#include "decoders/audio/audio_struct.h"
#include "openal/qtvsound.h"
#include "openal/qtvaudiodevice.h"
#include "core/datapacket/audio_data_packet.h"


class CLAbstractAudioDecoder;
class QnCompressedAudioData;

/*!
    \note On source data end \a playCurrentBuffer method MUST be called to avoid media data loss
*/
class QnAudioStreamDisplay : public QObject
{
    Q_OBJECT
public:
    QnAudioStreamDisplay(int buffMs, int prebufferMs);
    ~QnAudioStreamDisplay();

    void putData(QnCompressedAudioDataPtr data, qint64 minTime = 0);
    void enqueueData(QnCompressedAudioDataPtr data, qint64 minTime = 0);
    void suspend();

    void resume();

    // removes all data from audio buffers
    void clearAudioBuffer();

    // clears only device buff, not packets queue
    void clearDeviceBuffer();

    // how many ms is buffered in audio buffers at this moment(!)
    int msInBuffer() const;

    // returns start buffering time or AV_NOPTS_VALUE if audio is not buffering
    qint64 startBufferingTime() const;

    // returns false if format is not supported 
    bool isFormatSupported() const;
    
    // forcing downmixing, even if output device supports multichannel output
    void setForceDownmix(bool value);

    bool isDownmixForced() const { return m_forceDownmix; }

    //!Plays current buffer to the end
    void playCurrentBuffer();

    int getAudioBufferSize() const;

private:
    int msInQueue() const;

    bool initFormatConvertRule(QnAudioFormat format);

private:
    // TODO: #Elric #enum
    enum SampleConvertMethod {SampleConvert_None, SampleConvert_Float2Int32, SampleConvert_Float2Int16, SampleConvert_Int32ToInt16};

    QnMutex m_guiSync;
    QMap<CodecID, CLAbstractAudioDecoder*> m_decoder;

    int m_bufferMs;
    int m_prebufferMs;
    bool m_tooFewDataDetected;
    bool m_isFormatSupported;
    QtvSound* m_audioSound;

    bool m_downmixing;    // do downmix.
    bool m_forceDownmix;  // force downmix, even if output device supports multichannel
    SampleConvertMethod m_sampleConvertMethod;
    bool m_isConvertMethodInitialized;

    QQueue<QnCompressedAudioDataPtr> m_audioQueue;
    QnByteArray m_decodedAudioBuffer;
    qint64 m_startBufferingTime;
};

#endif //QN_AUDIO_STREAM_DISPLAY
