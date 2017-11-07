#ifndef THUMBNAILS_STREAM_READER_H__
#define THUMBNAILS_STREAM_READER_H__

#ifdef ENABLE_DATA_PROVIDERS
extern "C"
{
#include <libavformat/avformat.h>
};

#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <core/resource/resource_media_layout.h>
#include <nx/streaming/abstract_archive_delegate.h>

/*
* QnThumbnailsStreamReader gets frame sequence in the selected range with defined step.
*/

class QnThumbnailsStreamReader : public QnAbstractMediaStreamDataProvider
{
    Q_OBJECT;

public:
    QnThumbnailsStreamReader(const QnResourcePtr& dev, QnAbstractArchiveDelegate* archiveDelegate);
    virtual ~QnThumbnailsStreamReader();

    /*
    * Frame step in mks
    */
    void setRange(qint64 startTime, qint64 endTime, qint64 frameStep, int cseq);

    void setQuality(MediaQuality q);
    void setGroupId(const QByteArray& groupId);
protected:
    virtual QnAbstractMediaDataPtr getNextData();
    virtual void run();
    virtual void afterRun() override;
private:
    QnAbstractMediaDataPtr createEmptyPacket();
private:
    QnAbstractArchiveDelegate* m_archiveDelegate;
    QnAbstractArchiveDelegate* m_delegate;
    int m_cseq;
};

#endif //ENABLE_DATA_PROVIDERS

#endif //THUMBNAILS_STREAM_READER_H__
