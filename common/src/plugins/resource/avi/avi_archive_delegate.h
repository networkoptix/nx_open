#ifndef __AVI_ARCHIVE_DELEGATE_H
#define __AVI_ARCHIVE_DELEGATE_H

#ifdef ENABLE_ARCHIVE

#include <QtCore/QSharedPointer>

#include "core/datapacket/audio_data_packet.h"
#include "core/datapacket/video_data_packet.h"

#include <plugins/resource/archive/abstract_archive_delegate.h>

extern "C"
{
    #include <libavformat/avformat.h>
}

struct AVFormatContext;
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
    virtual qint64 startTime();
    virtual qint64 endTime();
    virtual QnAbstractMediaDataPtr getNextData();
    virtual qint64 seek (qint64 time, bool findIFrame);
    virtual QnResourceVideoLayoutPtr getVideoLayout() override;
    virtual QnResourceAudioLayoutPtr getAudioLayout() override;

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
    void packetTimestamp(QnCompressedAudioData* audio, const AVPacket& packet);
    void packetTimestamp(QnCompressedVideoData* video, const AVPacket& packet);
    void initLayoutStreams();
    AVFormatContext* getFormatContext();
private:
    bool deserializeLayout(QnCustomResourceVideoLayout* layout, const QString& layoutStr);
    QnMediaContextPtr getCodecContext(AVStream* stream);
protected:
    AVFormatContext* m_formatContext;
    QnResourcePtr m_resource;
    qint64 m_startMksec;
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
    QList<QnMediaContextPtr> m_contexts;

    qint64 m_startTime;
    bool m_useAbsolutePos;
    qint64 m_duration;

    friend class QnAviAudioLayout;
    AVIOContext* m_ioContext;
    bool m_eofReached;
    QMutex m_openMutex;
    QVector<qint64> m_lastPacketTimes;
    bool m_fastStreamFind;
};

typedef QSharedPointer<QnAviArchiveDelegate> QnAviArchiveDelegatePtr;

#endif // ENABLE_ARCHIVE

#endif
