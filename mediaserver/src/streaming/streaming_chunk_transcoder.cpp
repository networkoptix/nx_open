////////////////////////////////////////////////////////////
// 18 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "streaming_chunk_transcoder.h"

#include <QMutexLocker>

#include <core/resource_managment/resource_pool.h>
#include <core/resource/media_resource.h>
#include <plugins/resources/archive/abstract_archive_stream_reader.h>
#include <transcoding/ffmpeg_transcoder.h>
#include <utils/common/log.h>
#include <utils/common/timermanager.h>

#include "ondemand_media_data_provider.h"
#include "live_media_cache_reader.h"
#include "streaming_chunk_cache_key.h"


using namespace std;

//!Maximum time (in seconds) by which requested chunk start time may be in future
static const double MAX_CHUNK_TIMESTAMP_ADVANCE = 30;
static const int TRANSCODE_THREAD_COUNT = 4;

StreamingChunkTranscoder::StreamingChunkTranscoder( Flags flags )
:
    m_flags( flags ),
    m_newTranscodeID( 1 )
{
    //TODO/IMPL
}

bool StreamingChunkTranscoder::transcodeAsync(
    const StreamingChunkCacheKey& transcodeParams,
    StreamingChunk* const chunk )
{
    //searching for resource
    QnResourcePtr resource = QnResourcePool::instance()->getResourceByUniqId( transcodeParams.srcResourceUniqueID() );
    if( !resource )
    {
        NX_LOG( QString::fromAscii("StreamingChunkTranscoder::transcodeAsync. Requested resource %1 not found").
            arg(transcodeParams.srcResourceUniqueID()), cl_logDEBUG1 );
        return false;
    }

    QnMediaResourcePtr mediaResource = resource.dynamicCast<QnMediaResource>();
    if( !mediaResource )
    {
        NX_LOG( QString::fromAscii("StreamingChunkTranscoder::transcodeAsync. Requested resource %1 is not media resource").
            arg(transcodeParams.srcResourceUniqueID()), cl_logDEBUG1 );
        return false;
    }

    //validating transcoding parameters
    if( !validateTranscodingParameters( transcodeParams ) )
        return false;
    //if region is in future not futher, than MAX_CHUNK_TIMESTAMP_ADVANCE: scheduling task

    Q_ASSERT( transcodeParams.startTimestamp() <= transcodeParams.endTimestamp() );

    pair<map<int, TranscodeContext>::iterator, bool> p = m_transcodings.insert(
        make_pair( m_newTranscodeID.fetchAndAddAcquire(1), TranscodeContext() ) );  //TODO/IMPL
    Q_ASSERT( p.second );

    //checking requested time region:
        //whether data is present (in archive or cache)
    if( mediaResource->liveCache() )
    {
        const QDateTime& cacheStartTimestamp = mediaResource->liveCache()->startTime();
        const QDateTime& cacheEndTimestamp = mediaResource->liveCache()->endTime();
        QSharedPointer<LiveMediaCacheReader> liveMediaCacheReader( new LiveMediaCacheReader( mediaResource->liveCache() ) );
        if( transcodeParams.startTimestamp() < cacheEndTimestamp &&
            transcodeParams.endTimestamp() > cacheStartTimestamp )
        {
            //requested data is in live cache (at least, partially)
            if( startTranscoding(
                    p.first->first,
                    mediaResource,
                    liveMediaCacheReader,
                    transcodeParams,
                    chunk ) )
            {
                chunk->openForModification();
                return true;
            }
            m_transcodings.erase( p.first );
            return false;
        }

        if( transcodeParams.startTimestamp() > cacheEndTimestamp &&
            cacheEndTimestamp.msecsTo(transcodeParams.startTimestamp()) < MAX_CHUNK_TIMESTAMP_ADVANCE )
        {
            //chunk is in future not futher, than MAX_CHUNK_TIMESTAMP_ADVANCE
            if( scheduleTranscoding(
                    p.first->first,
                    transcodeParams.startTimestamp() ) )
            {
                chunk->openForModification();
                return true;
            }
            m_transcodings.erase( p.first );
            return false;
        }
    }

    //checking archive
    QSharedPointer<QnAbstractStreamDataProvider> dp( mediaResource->createDataProvider( QnResource::Role_LiveVideo ) );
    if( !dp )
    {
        NX_LOG( QString::fromLatin1("StreamingChunkTranscoder::transcodeAsync. Failed (1) to create archive data provider (resource %1)").
            arg(transcodeParams.srcResourceUniqueID()), cl_logWARNING );
        m_transcodings.erase( p.first );
        return false;
    }

    QSharedPointer<QnAbstractArchiveReader> archiveReader = dp.dynamicCast<QnAbstractArchiveReader>();
    if( !archiveReader )
    {
        NX_LOG( QString::fromLatin1("StreamingChunkTranscoder::transcodeAsync. Failed (2) to create archive data provider (resource %1)").
            arg(transcodeParams.srcResourceUniqueID()), cl_logWARNING );
        m_transcodings.erase( p.first );
        return false;
    }

    QSharedPointer<OnDemandMediaDataProvider> onDemandArchiveReader( new OnDemandMediaDataProvider( dp ) );
    if( transcodeParams.startTimestamp().toMSecsSinceEpoch() < (archiveReader->endTime() / 1000) &&
        transcodeParams.endTimestamp().toMSecsSinceEpoch() > (archiveReader->startTime() / 1000) )
    {
        if( startTranscoding(
                p.first->first,
                mediaResource,
                onDemandArchiveReader,
                transcodeParams,
                chunk ) )
        {
            chunk->openForModification();
            return true;
        }

        m_transcodings.erase( p.first );
        return false;
    }

    NX_LOG( QString::fromLatin1("StreamingChunkTranscoder::transcodeAsync. Failed to find source. "
        "Resource %1, statTime %2, duration %3").arg(mediaResource->getUniqueId()).
        arg(transcodeParams.startTimestamp().toString()).arg(transcodeParams.endTimestamp().toString()), cl_logWARNING );

    m_transcodings.erase( p.first );
    return false;
}

Q_GLOBAL_STATIC_WITH_ARGS( StreamingChunkTranscoder, streamingChunkTranscoderInstance, (StreamingChunkTranscoder::fBeginOfRangeInclusive) );

StreamingChunkTranscoder* StreamingChunkTranscoder::instance()
{
    return streamingChunkTranscoderInstance();
}

void StreamingChunkTranscoder::onTimer( const quint64& timerID )
{
    QMutexLocker lk( &m_mutex );

    std::map<quint64, int>::const_iterator taskIter = m_taskIDToTranscode.find( timerID );
    if( taskIter == m_taskIDToTranscode.end() )
    {
        NX_LOG( QString("StreamingChunkTranscoder::onTimer. Received timer event with unknown id %1. Ignoring...").arg(timerID), cl_logDEBUG1 );
        return;
    }

    const int transcodeID = taskIter->second;
    m_taskIDToTranscode.erase( taskIter );

    std::map<int, TranscodeContext>::const_iterator transcodeIter = m_transcodings.find( transcodeID );
    if( transcodeIter == m_transcodings.end() )
    {
        NX_LOG( QString("StreamingChunkTranscoder::onTimer. Received timer event (%1) with unknown transcode id %2. Ignoring...").
            arg(timerID).arg(transcodeID), cl_logDEBUG1 );
        return;
    }

    NX_LOG( QString("StreamingChunkTranscoder::onTimer. Received timer event with id %1. Resource %1").
        arg(timerID).arg(transcodeIter->second.mediaResource->getUniqueId()), cl_logDEBUG1 );

    //starting transcoding
    if( !startTranscoding(
            transcodeIter->second.mediaResource,
            transcodeIter->second.dataSource,
            transcodeIter->second.transcodeParams,
            transcodeIter->second.chunk ) )
    {
        NX_LOG( QString::fromLatin1("StreamingChunkTranscoder::onTimer. Failed to start transcoding (resource %1)").
            arg(transcodeIter->second.mediaResource->getUniqueId()), cl_logWARNING );
        transcodeIter->second.chunk->doneModification( StreamingChunk::rcError );
    }
}

bool StreamingChunkTranscoder::startTranscoding(
    int transcodingID,
    const QnMediaResourcePtr& mediaResource,
    QSharedPointer<AbstractOnDemandDataProvider> dataSource,
    const StreamingChunkCacheKey& transcodeParams,
    StreamingChunk* const chunk )
{
    //launching transcoding:
        //creating transcoder
    std::auto_ptr<QnFfmpegTranscoder> transcoder( new QnFfmpegTranscoder() );
    if( transcoder->setContainer( transcodeParams.containerFormat() ) != 0 )
    {
        NX_LOG( QString::fromLatin1("Failed to create transcoder with container \"%1\" to transcode chunk (%2 - %3) of resource %4").
            arg(transcodeParams.containerFormat()).arg(transcodeParams.startTimestamp().toString()).
            arg(transcodeParams.endTimestamp().toString()).arg(transcodeParams.srcResourceUniqueID()), cl_logWARNING );
        return false;
    }
    //TODO/IMPL set correct video parameters
    if( transcoder->setVideoCodec( CODEC_ID_H264, QnTranscoder::TM_DirectStreamCopy ) != 0 )
    {
        NX_LOG( QString::fromLatin1("Failed to create transcoder with video codec \"%1\" to transcode chunk (%2 - %3) of resource %4").
            arg(transcodeParams.videoCodec()).arg(transcodeParams.startTimestamp().toString()).
            arg(transcodeParams.endTimestamp().toString()).arg(transcodeParams.srcResourceUniqueID()), cl_logWARNING );
        return false;
    }
    if( transcoder->setAudioCodec( CODEC_ID_AAC, QnTranscoder::TM_FfmpegTranscode ) != 0 )
    {
        NX_LOG( QString::fromLatin1("Failed to create transcoder with audio codec \"%1\" to transcode chunk (%2 - %3) of resource %4").
            arg(transcodeParams.audioCodec()).arg(transcodeParams.startTimestamp().toString()).
            arg(transcodeParams.endTimestamp().toString()).arg(transcodeParams.srcResourceUniqueID()), cl_logWARNING );
        return false;
    }

    //selecting least used transcoding thread from pool
    StreamingChunkTranscoderThread* transcoderThread = NULL;

    //adding transcoder to transcoding thread
    transcoderThread->startTranscoding(
        transcodingID,
        chunk,
        dataSource,
        transcodeParams,
        transcoder.release() );

    return true;
}

bool StreamingChunkTranscoder::scheduleTranscoding(
    const int transcodeID,
    const QDateTime& startTimeUTC )
{
    const time_t curTime = ::time(NULL);
    const quint64 taskID = TimerManager::instance()->addTimer(
        this,
        startTimeUTC.toTime_t() > curTime ? startTimeUTC.toTime_t() - curTime : 0 );

    QMutexLocker lk( &m_mutex );
    m_taskIDToTranscode[taskID] = transcodeID;
    return true;
}

bool StreamingChunkTranscoder::validateTranscodingParameters( const StreamingChunkCacheKey& transcodeParams )
{
    //TODO/IMPL
    return true;
}

void StreamingChunkTranscoder::threadFunc()
{
    //TODO/IMPL
}
