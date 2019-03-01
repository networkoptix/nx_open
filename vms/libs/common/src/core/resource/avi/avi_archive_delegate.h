#pragma once

#include <QtCore/QSharedPointer>

#include <core/resource/avi/avi_archive_metadata.h>

#include "nx/streaming/audio_data_packet.h"
#include "nx/streaming/video_data_packet.h"

#include <nx/streaming/abstract_archive_delegate.h>

#include <nx/utils/thread/mutex.h>

#include <core/resource/motion_window.h>

extern "C"
{
// For typedef struct AVIOContext.
#include <libavformat/avio.h>
};

struct AVPacket;
struct AVCodecContext;
struct AVFormatContext;
struct AVStream;
class QnCustomResourceVideoLayout;
class QnAviAudioLayout;

class QnAviArchiveDelegate: public QnAbstractArchiveDelegate
{
    Q_OBJECT

    friend class QnAviAudioLayout;
public:
    QnAviArchiveDelegate();
    virtual ~QnAviArchiveDelegate() override;

    virtual bool open(
        const QnResourcePtr& resource,
        AbstractArchiveIntegrityWatcher* archiveIntegrityWatcher = nullptr) override;
    virtual void close() override;
    virtual qint64 startTime() const override;
    virtual void setStartTimeUs(qint64 startTimeUs);
    virtual qint64 endTime() const override;
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual qint64 seek (qint64 time, bool findIFrame) override;
    virtual QnConstResourceVideoLayoutPtr getVideoLayout() override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout() override;
    virtual bool hasVideo() const override;

    virtual bool setAudioChannel(unsigned num) override;

    // for optimization
    //void doNotFindStreamInfo();
    void setFastStreamFind(bool value);
    bool isStreamsFound() const;
    void setUseAbsolutePos(bool value);
    void setStorage(const QnStorageResourcePtr &storage);
    virtual QnAbstractMotionArchiveConnectionPtr getMotionConnection(int channel) override;
    virtual void setMotionRegion(const QnMotionRegion& region) override;

    //void setMotionConnection(QnAbstractMotionArchiveConnectionPtr connection, int channel);
    virtual bool findStreams();

    const char* getTagValue( const char* tagName );

    QnAviArchiveMetadata metadata() const;

protected:
    qint64 packetTimestamp(const AVPacket& packet);
    void packetTimestamp(QnCompressedVideoData* video, const AVPacket& packet);
    void initLayoutStreams();
    bool initMetadata();
    AVFormatContext* getFormatContext();

private:
    QnConstMediaContextPtr getCodecContext(AVStream* stream);
    bool reopen();
    void fixG726Bug();
protected:
    AVFormatContext* m_formatContext = nullptr;
    AVIOContext* m_IOContext = nullptr;
    QnResourcePtr m_resource;
    unsigned m_selectedAudioChannel = 0;
    bool m_initialized = false;
    QnStorageResourcePtr m_storage;

private:
    int m_audioStreamIndex = -1;
    int m_firstVideoIndex = 0;
    bool m_streamsFound = false;
    QnCustomResourceVideoLayoutPtr m_videoLayout;
    QnResourceAudioLayoutPtr m_audioLayout;
    QVector<int> m_indexToChannel;
    QList<QnConstMediaContextPtr> m_contexts;
    QnAviArchiveMetadata m_metadata;

    qint64 m_startTimeUs = 0;
    bool m_useAbsolutePos = true;
    qint64 m_durationUs = AV_NOPTS_VALUE;

    bool m_eofReached = false;
    QnMutex m_openMutex;
    QVector<qint64> m_lastPacketTimes;
    bool m_fastStreamFind = false;
    bool m_hasVideo = true;
    qint64 m_lastSeekTime = AV_NOPTS_VALUE;
    qint64 m_firstDts = 0; //< DTS of the very first video packet
    AbstractArchiveIntegrityWatcher* m_archiveIntegrityWatcher = nullptr;
    /**
     * Designates if a key frame has been found for a channel, that is, if
     * m_keyFrameFound[0] == true, than we have already found a key frame for the channel 0,
     * otherwise, we haven't.
     */
    std::vector<bool> m_keyFrameFound;
};

typedef QSharedPointer<QnAviArchiveDelegate> QnAviArchiveDelegatePtr;
