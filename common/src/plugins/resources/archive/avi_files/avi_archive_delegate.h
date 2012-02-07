#ifndef __AVI_ARCHIVE_DELEGATE_H
#define __AVI_ARCHIVE_DELEGATE_H

#include <QSharedPointer>
#include "../abstract_archive_delegate.h"

struct AVFormatContext;
class CLCustomDeviceVideoLayout;
class QnAviAudioLayout;

class QnAviArchiveDelegate: public QnAbstractArchiveDelegate
{
    Q_OBJECT;

public:
    QnAviArchiveDelegate();
    virtual ~QnAviArchiveDelegate();

    virtual bool open(QnResourcePtr resource);
    virtual void close();
    virtual qint64 startTime();
    virtual qint64 endTime();
    virtual QnAbstractMediaDataPtr getNextData();
    virtual qint64 seek (qint64 time, bool findIFrame);
    virtual QnVideoResourceLayout* getVideoLayout();
    virtual QnResourceAudioLayout* getAudioLayout();

    virtual AVCodecContext* setAudioChannel(int num);

    // for optimization       
    void doNotFindStreamInfo();
    bool isStreamsFound() const;
    void setUseAbsolutePos(bool value);
protected:
    virtual qint64 contentLength() const;
    virtual qint64 packetTimestamp(const AVPacket& packet);
    virtual bool findStreams();
    void initLayoutStreams();
    AVFormatContext* getFormatContext();
private:
    bool deserializeLayout(CLCustomDeviceVideoLayout* layout, const QString& layoutStr);
    QnMediaContextPtr getCodecContext(AVStream* stream);
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
    QnResourceAudioLayout* m_audioLayout;
    QVector<int> m_indexToChannel;
    QList<QnMediaContextPtr> m_contexts;

    qint64 m_startTime;
    bool m_useAbsolutePos;

    friend class QnAviAudioLayout;
};

typedef QSharedPointer<QnAviArchiveDelegate> QnAviArchiveDelegatePtr;

#endif
