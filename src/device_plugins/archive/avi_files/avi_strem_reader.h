#ifndef avi_stream_reader_h1901
#define avi_stream_reader_h1901

#include "../abstract_archive_stream_reader.h"
#include "data/mediadata.h"
#include "base/ffmpeg_helper.h"

struct AVFormatContext;

class CLAVIStreamReader : public CLAbstractArchiveReader
{
public:
    CLAVIStreamReader(CLDevice* dev);
    virtual ~CLAVIStreamReader();

    virtual quint64 currentTime() const;

    void previousFrame(quint64 mksec);

    virtual QStringList getAudioTracksInfo() const;
    virtual unsigned int getCurrentAudioChannel() const;
    virtual bool setAudioChannel(unsigned int num);
    virtual void setReverseMode(bool value);
    virtual bool isReverseMode() const { return m_reverseMode;}
    virtual bool isSpeedSupported(double value) const;

    CLDeviceVideoLayout* getLayout();
    static bool deserializeLayout(CLCustomDeviceVideoLayout* layout, const QString& layoutStr);
    static QString serializeLayout(const CLDeviceVideoLayout* layout);
protected:
    virtual CLAbstractMediaData* getNextData();
    virtual void channeljumpTo(quint64 mksec, int channel);

    virtual bool init();
    virtual void destroy();

    void smartSleep(qint64 mksec);
    virtual ByteIOContext* getIOContext();

    virtual qint64 packetTimestamp(AVStream* stream, const AVPacket& packet);
    virtual AVFormatContext* getFormatContext();
    virtual qint64 contentLength() const { return m_formatContext->duration; }
    bool initCodecs();
    bool openFormatContext();
protected:
    qint64 m_currentTime;
    qint64 m_previousTime;
    qint64 m_topIFrameTime;
    qint64 m_bottomIFrameTime;
    

    AVFormatContext* m_formatContext;

    int m_primaryVideoIdx;
    int m_audioStreamIndex;

    CodecID m_videoCodecId;
    CodecID m_audioCodecId;

    int m_freq;
    int m_channels;

    bool mFirstTime;

    volatile bool m_wakeup;

    bool m_bsleep;
    QVector<bool> m_needSetReverseFlag;
private:
    AVPacket m_packets[2];
    int m_currentPacketIndex;
    bool m_haveSavedPacket;
    int m_selectedAudioChannel;
    static QMutex avi_mutex;
    static QSemaphore aviSemaphore ;
    bool m_eof;
    bool m_reverseMode;
    bool m_prevReverseMode;
    FrameTypeExtractor* m_frameTypeExtractor;
    qint64 m_lastGopSeekTime;
    QVector<qint64> m_lastPacketTimes;
    bool m_IFrameAfterJumpFound;
    qint64 m_requiredJumpTime;
    qint64 m_lastUIJumpTime;
    qint64 m_lastFrameDuration;
    CLCustomDeviceVideoLayout* m_layout;
    QVector<int> m_indexToChannel;
private:
    /**
      * Read next packet from file
      *
      * @param packet packet to store data to
      *
      * @return true if successful
      */
    bool getNextPacket(AVPacket& packet);

    /**
      * Read next video packet from file ignoring audio packets.
      * Store it to packet referenced by nextPacket()
      * 
      * @return true if successful
      */
    bool getNextVideoPacket();

    /**
      * Create CLCompressedVideoData object and fill it from StreamReader state, packet and codec context
      *
      * @return created object
      */
    CLCompressedVideoData* getVideoData(const AVPacket& packet, AVCodecContext* codecContext, int channel);

    /**
      * Create CLCompressedAudioData object and fill it from StreamReader state, packet and AVStream
      *
      * @return created object
      */
    CLCompressedAudioData* getAudioData(const AVPacket& packet, AVStream* stream);

    /**
      * Replace current and next packet references.
      */
    void switchPacket();

    AVPacket& currentPacket();
    AVPacket& nextPacket();
    qint64 determineDisplayTime();
    void intChanneljumpTo(quint64 mksec, int channel);
    int streamIndexToChannel(int index);
};

#endif //avi_stream_reader_h1901
