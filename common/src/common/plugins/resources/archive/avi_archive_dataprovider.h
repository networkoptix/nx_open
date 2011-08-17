#ifndef AVI_ARCHIVE_DATAPROVIDER_H
#define AVI_ARCHIVE_DATAPROVIDER_H

#include <QtCore/QMutex>
#include <QtCore/QSemaphore>

#include "datapacket/mediadatapacket.h"
#include "dataprovider/navigated_dataprovider.h"
#include "resource/media_resource_layout.h"

struct AVCodecContext;
struct AVFormatContext;
struct AVPacket;
struct AVStream;

class QnAviArchiveDataProvider: public QnNavigatedDataProvider
{
public:
    QnAviArchiveDataProvider(QnResourcePtr ptr);
    virtual ~QnAviArchiveDataProvider();

protected:
    virtual QnAbstractDataPacketPtr getNextData();
    virtual bool init();
    virtual void destroy();
    virtual AVFormatContext *getFormatContext();
    virtual qint64 contentLength() const { return m_formatContext->duration; }
    bool initCodecs();
    virtual void channeljumpTo(quint64 mksec, int /*channel*/);
    virtual void setSkipFramesToTime(quint64 skip);

private:
    AVPacket &currentPacket();
    AVPacket &nextPacket();
    bool getNextPacket(AVPacket &packet);
    void switchPacket();
    qint64 packetTimestamp(AVStream *stream, const AVPacket &packet);
    QnCompressedVideoDataPtr getVideoData(const AVPacket &packet, AVCodecContext *codecContext);
    QnCompressedAudioDataPtr getAudioData(const AVPacket &packet, AVStream *stream);
    bool getNextVideoPacket();

protected:
    qint64 m_currentTime;
    qint64 m_previousTime;

    AVFormatContext *m_formatContext;

    int m_videoStreamIndex;
    int m_audioStreamIndex;

    int m_videoCodecId;
    int m_audioCodecId;

    int m_freq;
    int m_channels;
    bool mFirstTime;
    volatile bool m_wakeup;
    bool m_bsleep;

    AVPacket *m_packets[2];
    int m_currentPacketIndex;
    bool m_haveSavedPacket;
    int m_selectedAudioChannel;
    static QMutex avi_mutex;
    static QSemaphore aviSemaphore ;
    bool m_eof;
    bool m_skippedToTime[CL_MAX_CHANNELS];
};

#endif // AVI_ARCHIVE_DATAPROVIDER_H
