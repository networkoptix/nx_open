#ifndef THUMBNAILS_STREAM_READER_H__
#define THUMBNAILS_STREAM_READER_H__

#include "core/dataprovider/media_streamdataprovider.h"
#include "core/resource/resource_media_layout.h"
#include <libavformat/avformat.h>
#include "plugins/resource/archive/abstract_archive_delegate.h"
#include "recorder/device_file_catalog.h"
#include "plugins/resource/server_archive/server_archive_delegate.h"

/*
* QnThumbnailsStreamReader gets frame sequence in the selected range with defined step.
*/

class QnThumbnailsStreamReader : public QnAbstractMediaStreamDataProvider
{
    Q_OBJECT;

public:
    QnThumbnailsStreamReader(const QnResourcePtr& dev);
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
    QnAbstractArchiveDelegate* m_delegate;
    qint64 m_currentPos;
    int m_cseq;
    QnAbstractArchiveDelegate* m_archiveDelegate;
};

#endif //THUMBNAILS_STREAM_READER_H__
