#pragma once

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
