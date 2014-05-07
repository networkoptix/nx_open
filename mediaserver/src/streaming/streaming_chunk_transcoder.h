////////////////////////////////////////////////////////////
// 14 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef STREAMINGCHUNKTRANSCODER_H
#define STREAMINGCHUNKTRANSCODER_H

#include <map>
#include <vector>

#include <QAtomicInt>
#include <QMutex>
#include <QThread>

#include <core/resource/media_resource.h>
#include <utils/common/timermanager.h>

#include "streaming_chunk_cache_key.h"


class AbstractOnDemandDataProvider;
class StreamingChunkCacheKey;
class StreamingChunk;
class StreamingChunkTranscoderThread;
class QnTranscoder;

//!Reads source stream (from archive or media cache) and transcodes it to required format
/*!
    All transcoding is performed asynchronously.
    Holds thread pool inside, which actually performes transcoding
*/
class StreamingChunkTranscoder
:
    public TimerEventHandler
{
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
        StreamingChunk* const chunk );

    static StreamingChunkTranscoder* instance();

protected:
    virtual void onTimer( const quint64& timerID );

private:
    struct TranscodeContext
    {
        QnSecurityCamResourcePtr mediaResource;
        QSharedPointer<AbstractOnDemandDataProvider> dataSource;
        StreamingChunkCacheKey transcodeParams;
        StreamingChunk* const chunk;
        QnTranscoder* transcoder;

        TranscodeContext();
    };

    Flags m_flags;
    QMutex m_mutex;
    //!map<transcoding id, data>
    std::map<int, TranscodeContext> m_transcodings;
    //!map<task id, transcoding id>
    std::map<quint64, int> m_taskIDToTranscode;
    QAtomicInt m_newTranscodeID;
    std::vector<StreamingChunkTranscoderThread*> m_transcodeThreads;

    bool startTranscoding(
        int transcodingID,
        const QnSecurityCamResourcePtr& mediaResource,
        QSharedPointer<AbstractOnDemandDataProvider> dataSource,
        const StreamingChunkCacheKey& transcodeParams,
        StreamingChunk* const chunk );
    bool scheduleTranscoding(
        const int transcodeID,
        int delaySec );
    bool validateTranscodingParameters( const StreamingChunkCacheKey& transcodeParams );
};

#endif  //STREAMINGCHUNKTRANSCODER_H
