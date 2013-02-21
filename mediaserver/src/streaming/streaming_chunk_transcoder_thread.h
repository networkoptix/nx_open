////////////////////////////////////////////////////////////
// 14 jan 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef STREAMING_CHUNK_TRANSCODER_THREAD_H
#define STREAMING_CHUNK_TRANSCODER_THREAD_H

#include <QMutex>
#include <QSharedPointer>
#include <QWaitCondition>

#include <core/dataprovider/abstract_ondemand_data_provider.h>
#include <utils/common/long_runnable.h>

#include "streaming_chunk_cache_key.h"


class QnFfmpegTranscoder;
class StreamingChunk;
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
        StreamingChunk* const chunk,
        QSharedPointer<AbstractOnDemandDataProvider> dataSource,
        const StreamingChunkCacheKey& transcodeParams,
        QnFfmpegTranscoder* transcoder );
    void cancel( int transcodingID );

public slots:
    virtual void pleaseStop() override;

protected:
    virtual void run() override;

protected slots:
    void onStreamDataAvailable( AbstractOnDemandDataProvider* pThis );

private:
    class TranscodeContext
    {
    public:
        StreamingChunk* const chunk;
        QSharedPointer<AbstractOnDemandDataProvider> dataSource;
        StreamingChunkCacheKey transcodeParams;
        QnFfmpegTranscoder* transcoder;
        bool dataAvailable;
        quint64 msTranscoded; 
        quint64 packetsTranscoded;
        //!-1, if no prev packet
        qint64 prevPacketTimestamp;

        TranscodeContext();
        TranscodeContext(
            StreamingChunk* const _chunk,
            QSharedPointer<AbstractOnDemandDataProvider> _dataSource,
            const StreamingChunkCacheKey& _transcodeParams,
            QnFfmpegTranscoder* _transcoder );
    };

    //map<transcodingID, TranscodeContext>
    std::map<int, TranscodeContext*> m_transcodeContext;
    //map<data source, transcoding id>
    std::map<AbstractOnDemandDataProvider*, int> m_dataSourceToID;
    QMutex m_mutex;
    QWaitCondition m_cond;

    void removeTranscodingNonSafe(
        const std::map<int, TranscodeContext*>::iterator& transcodingIter,
        bool transcodingFinishedSuccessfully );
};

#endif  //STREAMING_CHUNK_TRANSCODER_THREAD_H
