////////////////////////////////////////////////////////////
// 25 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef LIVE_MEDIA_CACHE_READER_H
#define LIVE_MEDIA_CACHE_READER_H

#include <core/dataprovider/abstract_ondemand_data_provider.h>


class MediaStreamCache;

class LiveMediaCacheReader
:
    public AbstractOnDemandDataProvider
{
public:
    LiveMediaCacheReader( const MediaStreamCache* mediaCache );

    virtual bool tryRead( QnAbstractDataPacketPtr* const data ) override;

private:
    const MediaStreamCache* m_mediaCache;
};

#endif  //LIVE_MEDIA_CACHE_READER_H
