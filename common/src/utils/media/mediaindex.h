////////////////////////////////////////////////////////////
// 14 jan 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#ifndef MEDIAINDEX_H
#define MEDIAINDEX_H

#include <vector>


class MediaIndex
{
public:
    class ChunkData
    {
    public:
        int mediaSequence;
        //!in msec
        int duration;
        //!in msec since epoch
        qint64 startTimestamp;

        ChunkData();
    };

    MediaIndex();

    bool generateChunkListForLivePlayback(
        int targetDurationMSec,
        int chunksToGenerate,
        std::vector<ChunkData>* const chunkList ) const;
};

#endif  //MEDIAINDEX_H
