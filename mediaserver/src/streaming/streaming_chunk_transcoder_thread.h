////////////////////////////////////////////////////////////
// 14 jan 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef STREAMING_CHUNK_TRANSCODER_THREAD_H
#define STREAMING_CHUNK_TRANSCODER_THREAD_H

#include <QElapsedTimer>
#include <utils/common/mutex.h>
#include <QSharedPointer>
#include <utils/common/wait_condition.h>

#include <core/dataprovider/abstract_ondemand_data_provider.h>
#include <utils/common/long_runnable.h>

#include "data_source_cache.h"
#include "streaming_chunk.h"
#include "streaming_chunk_cache_key.h"


class QnTranscoder;
class StreamingChunkCacheKey;

class StreamingChunkTranscoderThread
:
    public QnLongRunnable
{
    Q_OBJECT

public:
    StreamingChunkTranscoderThread();
    virtual ~StreamingChunkTranscoderThread();

    bool startTranscoding(
        int transcodingID,
        StreamingChunkPtr chunk,
        DataSourceContextPtr dataSourceCtx,
        const StreamingChunkCacheKey& transcodeParams );
    //void cancel( int transcodingID );

    size_t ongoingTranscodings() const;

public slots:
    virtual void pleaseStop() override;

protected:
    virtual void run() override;

protected slots:
    void onStreamDataAvailable( AbstractOnDemandDataProvider* pThis );

signals:
    void transcodingFinished(
        int transcodingID,
        bool result,
        const StreamingChunkCacheKey& key,
        DataSourceContextPtr data );

private:
    class TranscodeContext
    {
    public:
        StreamingChunkPtr chunk;
        DataSourceContextPtr dataSourceCtx;
        StreamingChunkCacheKey transcodeParams;
        bool dataAvailable;
        qint64 prevReadTryTimestamp;
        quint64 msTranscoded; 
        quint64 packetsTranscoded;
        //!-1, if no prev packet
        qint64 prevPacketTimestamp;

        //QFile outputFile;

        TranscodeContext();
        TranscodeContext(
            StreamingChunkPtr _chunk,
            DataSourceContextPtr dataSourceCtx,
            const StreamingChunkCacheKey& _transcodeParams );
    };

    //map<transcodingID, TranscodeContext>
    std::map<int, TranscodeContext*> m_transcodeContext;
    //map<data source, transcoding id>
    std::map<AbstractOnDemandDataProvider*, int> m_dataSourceToID;
    mutable QnMutex m_mutex;
    QnWaitCondition m_cond;
    QElapsedTimer m_monotonicClock;

    void removeTranscodingNonSafe(
        const std::map<int, TranscodeContext*>::iterator& transcodingIter,
        bool transcodingFinishedSuccessfully,
        QnMutexLocker* const lk );
};

#endif  //STREAMING_CHUNK_TRANSCODER_THREAD_H
