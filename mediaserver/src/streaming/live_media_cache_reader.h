////////////////////////////////////////////////////////////
// 25 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef LIVE_MEDIA_CACHE_READER_H
#define LIVE_MEDIA_CACHE_READER_H

#include <core/dataprovider/abstract_ondemand_data_provider.h>
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

private:
    MediaStreamCache::SequentialReadContext m_readCtx;
};

#endif  //LIVE_MEDIA_CACHE_READER_H
