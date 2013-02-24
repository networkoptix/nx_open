#ifndef __VMAX480_ARCHIVE_DELEGATE
#define __VMAX480_ARCHIVE_DELEGATE

#include "plugins/resources/archive/abstract_archive_delegate.h"
#include "vmax480_resource.h"
#include "vmax480_stream_fetcher.h"

class QnVMax480ArchiveDelegate: public QnAbstractArchiveDelegate, public VMaxStreamFetcher
{
public:
    QnVMax480ArchiveDelegate(QnResourcePtr res);
    virtual ~QnVMax480ArchiveDelegate();


    virtual bool open(QnResourcePtr resource)override;
    virtual void close()override;
    virtual qint64 startTime()override;
    virtual qint64 endTime()override;
    virtual QnAbstractMediaDataPtr getNextData()override;
    virtual qint64 seek (qint64 time, bool findIFrame)override;
    virtual QnResourceVideoLayout* getVideoLayout()override;
    virtual QnResourceAudioLayout* getAudioLayout()override;

    virtual void onGotData(QnAbstractMediaDataPtr mediaData) override;

    virtual void beforeClose() override;
private:
    QnPlVmax480ResourcePtr m_res;
    bool m_connected;
    CLDataQueue m_internalQueue;
    bool m_needStop;
    quint8 m_sequence;
};

#endif // __VMAX480_ARCHIVE_DELEGATE
