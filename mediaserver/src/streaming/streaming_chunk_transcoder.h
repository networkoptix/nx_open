////////////////////////////////////////////////////////////
// 14 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef STREAMINGCHUNKTRANSCODER_H
#define STREAMINGCHUNKTRANSCODER_H

#include <map>
#include <vector>

#include <QtCore/QAtomicInt>
#include <QtCore/QObject>
#include <utils/common/mutex.h>
#include <QtCore/QThread>

#include <core/dataprovider/abstract_ondemand_data_provider.h>
#include <core/resource/media_resource.h>
#include <streaming/ondemand_media_data_provider.h>
#include <utils/common/timermanager.h>

#include "data_source_cache.h"
#include "streaming_chunk.h"
#include "streaming_chunk_cache_key.h"


class StreamingChunkCacheKey;
class StreamingChunkTranscoderThread;
class QnTranscoder;

//!Reads source stream (from archive or media cache) and transcodes it to required format
/*!
    All transcoding is performed asynchronously.
    Holds thread pool inside, which actually performes transcoding
*/
class StreamingChunkTranscoder
:
    public QObject,
    public TimerEventHandler
{
    Q_OBJECT

public:
    enum Flags
    {
        //!Transcoding can be stopped only just before GOP
        fStopTranscodingAtGOPBoundary = 0x01,
        /*!
            Try to include begin of range in resulting chunk, i.e., start transcoding with GOP, including supplyed start timestamp. 
            Otherwise, transcoding is started with first GOP following start timestamp
        */
        fBeginOfRangeInclusive = 0x02,
        /*!
            Actual only with \a fStopTranscodingAtGOPBoundary specified.
            If specified, transcoding stops just before GOP, following specified ending timestamp. Otherwise, transcoding stops at GOP that includes end timestamp
        */
        fEndOfRangeInclusive = 0x04
    };

    /*!
        \param flags Combination of input flags
    */
    StreamingChunkTranscoder( Flags flags );
    ~StreamingChunkTranscoder();

    //!Starts transcoding of resource \a transcodeParams.srcResourceUniqueID
    /*!
        Requested data (\a transcodeParams.startTimestamp()) may be in future. In this case transcoding will start as soon as source data is available
        \param chunk Transcoded stream is passed to \a chunk->appendData
        \return false, if transcoding could not be started by any reason. true, otherwise
        \note \a chunk->openForModification is called in this method, not at actual transcoding start
        \note Transcoding can start only at GOP boundary (usually, that means with keyframe)
    */
    bool transcodeAsync(
        const StreamingChunkCacheKey& transcodeParams,
        StreamingChunkPtr chunk );

    static StreamingChunkTranscoder* instance();

protected:
    virtual void onTimer( const quint64& timerID );

private:
    struct TranscodeContext
    {
        QnSecurityCamResourcePtr mediaResource;
        DataSourceContextPtr dataSourceCtx;
        StreamingChunkCacheKey transcodeParams;
        StreamingChunkPtr chunk;
        QnTranscoder* transcoder;

        TranscodeContext();
    };

    bool m_terminated;
    Flags m_flags;
    QnMutex m_mutex;
    //!map<transcoding id, data>
    std::map<int, TranscodeContext> m_scheduledTranscodings;
    //!map<task id, transcoding id>
    std::map<quint64, int> m_taskIDToTranscode;
    QAtomicInt m_transcodeIDSeq;
    std::vector<StreamingChunkTranscoderThread*> m_transcodeThreads;
    DataSourceCache m_dataSourceCache;

    bool startTranscoding(
        int transcodingID,
        DataSourceContextPtr dataSourceCtx,
        const StreamingChunkCacheKey& transcodeParams,
        StreamingChunkPtr chunk );
    bool scheduleTranscoding(
        const int transcodeID,
        int delayMSec );
    bool validateTranscodingParameters( const StreamingChunkCacheKey& transcodeParams );
    QnTranscoder* createTranscoder(
        const QnSecurityCamResourcePtr& mediaResource,
        const StreamingChunkCacheKey& transcodeParams );

private slots:
    void onTranscodingFinished(
        int transcodingID,
        bool result,
        const StreamingChunkCacheKey& key,
        DataSourceContextPtr data );
};

#endif  //STREAMINGCHUNKTRANSCODER_H
