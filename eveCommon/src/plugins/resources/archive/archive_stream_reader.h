#ifndef avi_stream_reader_h1901
#define avi_stream_reader_h1901

#include "abstract_archive_stream_reader.h"
#include "core/resource/resource_media_layout.h"
#include "utils/media/ffmpeg_helper.h"


struct AVFormatContext;


class QnArchiveStreamReader : public QnAbstractArchiveReader
{
public:
    QnArchiveStreamReader(QnResource* dev);
    virtual ~QnArchiveStreamReader();

    virtual qint64 currentTime() const;

    void previousFrame(qint64 mksec);

    virtual QStringList getAudioTracksInfo() const;
    virtual unsigned int getCurrentAudioChannel() const;
    virtual bool setAudioChannel(unsigned int num);
    virtual void setReverseMode(bool value);
    virtual bool isReverseMode() const { return m_reverseMode;}
    virtual bool isNegativeSpeedSupported() const;

    virtual QnDeviceVideoLayout* getVideoLayout();
    virtual QnDeviceAudioLayout* getAudioLayout();
    static bool deserializeLayout(CLCustomDeviceVideoLayout* layout, const QString& layoutStr);
    static QString serializeLayout(const QnDeviceVideoLayout* layout);

    void renameFileOnDestroy(const QString& newFileName);
    qint64 startTime() const { return m_delegate->startTime(); }
    qint64 endTime() const { return m_delegate->endTime(); }
protected:
    virtual QnAbstractMediaDataPtr getNextData();
    virtual void channeljumpTo(qint64 mksec, int channel);

    virtual bool init();

    virtual ByteIOContext* getIOContext();

    virtual qint64 contentLength() const { return m_delegate->endTime() - m_delegate->startTime(); }
    bool initCodecs();
    bool openFormatContext();
protected:
    qint64 m_currentTime;
    qint64 m_previousTime;
    qint64 m_topIFrameTime;
    qint64 m_bottomIFrameTime;
    


    int m_primaryVideoIdx;
    int m_audioStreamIndex;

    CodecID m_videoCodecId;
    CodecID m_audioCodecId;

    int m_freq;
    int m_channels;

    bool mFirstTime;

    volatile bool m_wakeup;

private:
    int m_selectedAudioChannel;
    static QMutex avi_mutex;
    static QSemaphore aviSemaphore ;
    bool m_eof;
    bool m_reverseMode;
    bool m_prevReverseMode;
    FrameTypeExtractor* m_frameTypeExtractor;
    qint64 m_lastGopSeekTime;
    //QVector<qint64> m_lastPacketTimes;
    QVector<int> m_audioCodecs;
    bool m_IFrameAfterJumpFound;
    qint64 m_requiredJumpTime;
    qint64 m_lastUIJumpTime;
    qint64 m_lastFrameDuration;
    QString m_onDestroyFileName;
private:
    QMutex m_jumpMtx;
    QnAbstractMediaDataPtr m_currentData;
    QnAbstractMediaDataPtr m_nextData;

    qint64 determineDisplayTime();
    void intChanneljumpTo(qint64 mksec, int channel);
    bool getNextVideoPacket();
    void addAudioChannel(QnCompressedAudioDataPtr audio);
    QnAbstractMediaDataPtr getNextPacket();
};

#endif //avi_stream_reader_h1901
