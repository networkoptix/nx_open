////////////////////////////////////////////////////////////
// 14 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef STREAMINGCHUNKTRANSCODER_H
#define STREAMINGCHUNKTRANSCODER_H


class StreamingChunkCacheKey;
class StreamingChunk;

//!Reads source stream (from archive or media cache) and transcodes it to required format
/*!
    All transcoding is performed asynchronously.
    Holds thread pool inside, which actually performes transcoding
*/
class StreamingChunkTranscoder
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
};

#endif  //STREAMINGCHUNKTRANSCODER_H
