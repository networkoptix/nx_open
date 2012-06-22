#ifndef THUMBNAILS_STREAM_READER_H__
#define THUMBNAILS_STREAM_READER_H__

#include "core/dataprovider/media_streamdataprovider.h"
#include "core/resource/resource_media_layout.h"
#include <libavformat/avformat.h>
#include "plugins/resources/archive/abstract_archive_delegate.h"

/*
* QnThumbnailsStreamReader gets frame sequence in the selected range with defined step.
*/

class QnThumbnailsStreamReader : public QnAbstractMediaStreamDataProvider
{
    Q_OBJECT;

public:
    QnThumbnailsStreamReader(QnResourcePtr dev);
    virtual ~QnThumbnailsStreamReader();

    /*
    * Frame step in mks
    */
    void setRange(qint64 startTime, qint64 endTime, qint64 frameStep, int cseq);
protected:
    virtual QnAbstractMediaDataPtr getNextData() override;
    virtual void run();
private:
    QnAbstractMediaDataPtr createEmptyPacket();
private:
    QnAbstractArchiveDelegate* m_delegate;
    qint64 m_currentPos;
    int m_cseq;
};

#endif //THUMBNAILS_STREAM_READER_H__
