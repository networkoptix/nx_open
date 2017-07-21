#ifndef __AVI_ARCHIVE_DELEGATE_H
#define __AVI_ARCHIVE_DELEGATE_H

#include <QtCore/QSharedPointer>

#include "nx/streaming/audio_data_packet.h"
#include "nx/streaming/video_data_packet.h"

#include <nx/streaming/abstract_archive_delegate.h>

#include <nx/utils/thread/mutex.h>

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
    Q_OBJECT;

public:
    enum Tag {
        StartTimeTag,
        EndTimeTag,
        LayoutInfoTag,
        SoftwareTag,
        SignatureTag,
        DewarpingTag,
        CustomTag /**< Tag for all other future values encoded in JSON object. */
    };

    /*
    * Some containers supports only predefined tag names. So, I've introduce this function
    */
    static const char* getTagName(Tag tag, const QString& formatName);
    const char* getTagValue(Tag tag);

    QnAviArchiveDelegate();
    virtual ~QnAviArchiveDelegate();

    virtual bool open(const QnResourcePtr &resource);
    virtual void close();
    virtual qint64 startTime() const;
    virtual qint64 endTime() const;
    virtual QnAbstractMediaDataPtr getNextData();
    virtual qint64 seek (qint64 time, bool findIFrame);
    virtual QnConstResourceVideoLayoutPtr getVideoLayout() override;
    virtual QnConstResourceAudioLayoutPtr getAudioLayout() override;
    virtual bool hasVideo() const override;

    virtual AVCodecContext* setAudioChannel(int num);

    // for optimization
    //void doNotFindStreamInfo();
    void setFastStreamFind(bool value);
    bool isStreamsFound() const;
    void setUseAbsolutePos(bool value);
    void setStorage(const QnStorageResourcePtr &storage);
    virtual QnAbstractMotionArchiveConnectionPtr getMotionConnection(int channel) override;

    //void setMotionConnection(QnAbstractMotionArchiveConnectionPtr connection, int channel);
    virtual bool findStreams();

    const char* getTagValue( const char* tagName );

protected:
    qint64 packetTimestamp(const AVPacket& packet);
    void packetTimestamp(QnCompressedVideoData* video, const AVPacket& packet);
    void initLayoutStreams();
    AVFormatContext* getFormatContext();
private:
    bool deserializeLayout(QnCustomResourceVideoLayout* layout, const QString& layoutStr);
    QnConstMediaContextPtr getCodecContext(AVStream* stream);
    bool reopen();
protected:
    AVFormatContext* m_formatContext;
    QnResourcePtr m_resource;
    qint64 m_playlistOffsetUsec; // File additional offset inside playlist for DVD/BluRay
    int m_selectedAudioChannel;
    bool m_initialized;
    QnStorageResourcePtr m_storage;
private:
    int m_audioStreamIndex;
    int m_firstVideoIndex;
    bool m_streamsFound;
    QnCustomResourceVideoLayoutPtr m_videoLayout;
    QnResourceAudioLayoutPtr m_audioLayout;
    QVector<int> m_indexToChannel;
    QList<QnConstMediaContextPtr> m_contexts;

    qint64 m_startTimeUsec; //microseconds
    bool m_useAbsolutePos;
    qint64 m_duration;

    friend class QnAviAudioLayout;
    bool m_eofReached;
    QnMutex m_openMutex;
    QVector<qint64> m_lastPacketTimes;
    bool m_fastStreamFind;
    bool m_hasVideo;
    qint64 m_lastSeekTime;
};

typedef QSharedPointer<QnAviArchiveDelegate> QnAviArchiveDelegatePtr;

#endif
