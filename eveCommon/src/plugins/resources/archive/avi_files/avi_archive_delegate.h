#ifndef __AVI_ARCHIVE_DELEGATE_H
#define __AVI_ARCHIVE_DELEGATE_H

#include "../abstract_archive_delegate.h"


struct AVFormatContext;
class CLCustomDeviceVideoLayout;
class QnAviAudioLayout;

class QnAviArchiveDelegate: public QnAbstractArchiveDelegate
{
public:
    QnAviArchiveDelegate();
    virtual ~QnAviArchiveDelegate();

    virtual bool open(QnResourcePtr resource);
    virtual void close();
    virtual qint64 startTime();
    virtual qint64 endTime();
    virtual QnAbstractMediaDataPtr getNextData();
    virtual qint64 seek (qint64 time);
    virtual QnDeviceVideoLayout* getVideoLayout();
    virtual QnDeviceAudioLayout* getAudioLayout();
    
    virtual AVCodecContext* setAudioChannel(int num);
protected:
    virtual qint64 contentLength() const;
    virtual qint64 packetTimestamp(const AVPacket& packet);
    virtual bool findStreams();
    void initLayoutStreams();
    AVFormatContext* getFormatContext();
private:
    bool deserializeLayout(CLCustomDeviceVideoLayout* layout, const QString& layoutStr);
protected:
    AVFormatContext* m_formatContext;
    QnResourcePtr m_resource;
    qint64 m_startMksec;
    int m_selectedAudioChannel;
    bool m_initialized;
private:
    int m_audioStreamIndex;
    int m_firstVideoIndex;
    bool m_streamsFound;
    CLCustomDeviceVideoLayout* m_videoLayout;
    QnDeviceAudioLayout* m_audioLayout;
    QVector<int> m_indexToChannel;

    friend class QnAviAudioLayout;
};

#endif
