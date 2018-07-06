////////////////////////////////////////////////////////////
// 25 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef LIVE_MEDIA_CACHE_READER_H
#define LIVE_MEDIA_CACHE_READER_H

#include <stack>

#include <providers/abstract_ondemand_data_provider.h>
#include <utils/media/media_stream_cache.h>


class MediaStreamCache;

class LiveMediaCacheReader
:
    public AbstractOnDemandDataProvider
{
public:
    LiveMediaCacheReader( MediaStreamCache* mediaCache, quint64 startTimestamp );

    virtual bool tryRead( QnAbstractDataPacketPtr* const data ) override;
    virtual quint64 currentPos() const override;
    virtual void put(QnAbstractDataPacketPtr packet) override;

private:
    MediaStreamCache::SequentialReadContext m_readCtx;
    std::stack<QnAbstractDataPacketPtr> m_frontStack;
};

#endif  //LIVE_MEDIA_CACHE_READER_H
