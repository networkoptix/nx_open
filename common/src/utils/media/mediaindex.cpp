////////////////////////////////////////////////////////////
// 14 jan 2013    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "mediaindex.h"


MediaIndex::ChunkData::ChunkData()
:
    mediaSequence( 0 ),
    duration( 0 ),
    startTimestamp( 0 )
{
}

MediaIndex::MediaIndex()
{
}

bool MediaIndex::generateChunkListForLivePlayback(
    int targetDurationMSec,
    int chunksToGenerate,
    std::vector<ChunkData>* const chunkList ) const
{
    //TODO/IMPL
    return false;
}
