////////////////////////////////////////////////////////////
// 18 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "streaming_chunk_transcoder.h"

#include <QtCore/QMutexLocker>

#include <core/dataprovider/h264_mp4_to_annexb.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/security_cam_resource.h>
#include <plugins/resources/archive/abstract_archive_stream_reader.h>
#include <transcoding/ffmpeg_transcoder.h>
#include <utils/common/log.h>
#include <utils/common/timermanager.h>

#include "ondemand_media_data_provider.h"
#include "live_media_cache_reader.h"
#include "streaming_chunk_cache_key.h"
#include "streaming_chunk_transcoder_thread.h"
#include "streaming_chunk.h"
#include "../camera/camera_pool.h"


using namespace std;

//!Maximum time (in micros) by which requested chunk start time may be in future
static const double MAX_CHUNK_TIMESTAMP_ADVANCE_MICROS = 30*1000*1000;
static const int TRANSCODE_THREAD_COUNT = 1;
static const int USEC_IN_MSEC = 1000;
static const int MSEC_IN_SEC = 1000;
static const int MICROS_IN_SECOND = MSEC_IN_SEC*USEC_IN_MSEC;

StreamingChunkTranscoder::TranscodeContext::TranscodeContext()
:
    chunk( NULL ),
    transcoder( NULL )
{
}


StreamingChunkTranscoder::StreamingChunkTranscoder( Flags flags )
:
    m_flags( flags ),
    m_newTranscodeID( 1 )
{
    m_transcodeThreads.resize( TRANSCODE_THREAD_COUNT );
    for( size_t i = 0; i < m_transcodeThreads.size(); ++i )
    {
        m_transcodeThreads[i] = new StreamingChunkTranscoderThread();
        m_transcodeThreads[i]->start();
    }
}

StreamingChunkTranscoder::~StreamingChunkTranscoder()
{
    for( std::vector<StreamingChunkTranscoderThread*>::size_type
        i = 0;
        i < m_transcodeThreads.size();
        ++i )
    {
        delete m_transcodeThreads[i];
    }
    m_transcodeThreads.clear();
}

bool StreamingChunkTranscoder::transcodeAsync(
    const StreamingChunkCacheKey& transcodeParams,
    StreamingChunk* const chunk )
{
    //searching for resource
    QnResourcePtr resource = QnResourcePool::instance()->getResourceByUniqId( transcodeParams.srcResourceUniqueID() );
    if( !resource )
    {
        NX_LOG( QString::fromLatin1("StreamingChunkTranscoder::transcodeAsync. Requested resource %1 not found").
            arg(transcodeParams.srcResourceUniqueID()), cl_logDEBUG1 );
        return false;
    }

    QnSecurityCamResourcePtr cameraResource = resource.dynamicCast<QnSecurityCamResource>();
    if( !cameraResource )
    {
        NX_LOG( QString::fromLatin1("StreamingChunkTranscoder::transcodeAsync. Requested resource %1 is not media resource").
            arg(transcodeParams.srcResourceUniqueID()), cl_logDEBUG1 );
        return false;
    }

    QnVideoCamera* camera = qnCameraPool->getVideoCamera( resource );
    Q_ASSERT( camera );

    //validating transcoding parameters
    if( !validateTranscodingParameters( transcodeParams ) )
        return false;
    //if region is in future not futher, than MAX_CHUNK_TIMESTAMP_ADVANCE: scheduling task

    Q_ASSERT( transcodeParams.startTimestamp() <= transcodeParams.endTimestamp() );

    pair<map<int, TranscodeContext>::iterator, bool> p = m_transcodings.insert(
        make_pair( m_newTranscodeID.fetchAndAddAcquire(1), TranscodeContext() ) );  //TODO/HLS: #ak ???
    Q_ASSERT( p.second );

    //checking requested time region:
        //whether data is present (in archive or cache)
    if( transcodeParams.live() )
    {
        if( !camera->liveCache(transcodeParams.streamQuality()) )
            return false;

        const quint64 cacheStartTimestamp = camera->liveCache(transcodeParams.streamQuality())->startTimestamp();
        const quint64 cacheEndTimestamp = camera->liveCache(transcodeParams.streamQuality())->currentTimestamp();
        const quint64 actualStartTimestamp = std::max<>( cacheStartTimestamp, transcodeParams.startTimestamp() );
        QSharedPointer<LiveMediaCacheReader> liveMediaCacheReader( new LiveMediaCacheReader( camera->liveCache(transcodeParams.streamQuality()), actualStartTimestamp ) );
        if( transcodeParams.startTimestamp() < cacheEndTimestamp &&
            transcodeParams.endTimestamp() > cacheStartTimestamp )
        {
            //requested data is in live cache (at least, partially)
            if( startTranscoding(
                    p.first->first,
                    cameraResource,
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
            transcodeParams.startTimestamp() - cacheEndTimestamp < MAX_CHUNK_TIMESTAMP_ADVANCE_MICROS )
        {
            //chunk is in future not futher, than MAX_CHUNK_TIMESTAMP_ADVANCE_MICROS
            if( scheduleTranscoding(
                    p.first->first,
                    (transcodeParams.startTimestamp() - cacheEndTimestamp) / MICROS_IN_SECOND + 1 ) )
            {
                chunk->openForModification();
                return true;
            }
            m_transcodings.erase( p.first );
            return false;
        }

        return true;
    }

    //TODO/HLS: #ak optimization: take existing archive reader which is already at required position (from previous chunk)

    //creating archive reader
    QSharedPointer<QnAbstractStreamDataProvider> dp( cameraResource->createDataProvider( QnResource::Role_Archive ) );
    if( !dp )
    {
        NX_LOG( QString::fromLatin1("StreamingChunkTranscoder::transcodeAsync. Failed (1) to create archive data provider (resource %1)").
            arg(transcodeParams.srcResourceUniqueID()), cl_logWARNING );
        m_transcodings.erase( p.first );
        return false;
    }

    QSharedPointer<QnAbstractArchiveReader> archiveReader = dp.dynamicCast<QnAbstractArchiveReader>();
    if( !archiveReader || !archiveReader->open() )
    {
        NX_LOG( QString::fromLatin1("StreamingChunkTranscoder::transcodeAsync. Failed (2) to create archive data provider (resource %1)").
            arg(transcodeParams.srcResourceUniqueID()), cl_logWARNING );
        m_transcodings.erase( p.first );
        return false;
    }
    archiveReader->setQuality(transcodeParams.streamQuality(), true);
    archiveReader->setPlaybackRange( QnTimePeriod( transcodeParams.startTimestamp() / USEC_IN_MSEC, transcodeParams.duration() ) );

    if( !startTranscoding(
            p.first->first,
            cameraResource,
            QSharedPointer<OnDemandMediaDataProvider>( new OnDemandMediaDataProvider( dp ) ),
            transcodeParams,
            chunk ) )
    {
        m_transcodings.erase( p.first );
        return false;
    }

    chunk->openForModification();
    archiveReader->start();
    return true;
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
        arg(timerID).arg(transcodeIter->second.mediaResource->toResource()->getUniqueId()), cl_logDEBUG1 );

    //starting transcoding
    if( !startTranscoding(
            transcodeID,
            transcodeIter->second.mediaResource,
            transcodeIter->second.dataSource,
            transcodeIter->second.transcodeParams,
            transcodeIter->second.chunk ) )
    {
        NX_LOG( QString::fromLatin1("StreamingChunkTranscoder::onTimer. Failed to start transcoding (resource %1)").
            arg(transcodeIter->second.mediaResource->toResource()->getUniqueId()), cl_logWARNING );
        transcodeIter->second.chunk->doneModification( StreamingChunk::rcError );
    }
}

bool StreamingChunkTranscoder::startTranscoding(
    int transcodingID,
    const QnSecurityCamResourcePtr& mediaResource,
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
            arg(transcodeParams.containerFormat()).arg(transcodeParams.startTimestamp()).
            arg(transcodeParams.endTimestamp()).arg(transcodeParams.srcResourceUniqueID()), cl_logWARNING );
        return false;
    }
    //TODO/HLS: #ak setting correct video parameters, now they are hard-coded for HLS with no transcoding
    CodecID codecID = CODEC_ID_NONE;
    QnTranscoder::TranscodeMethod transcodeMethod = QnTranscoder::TM_DirectStreamCopy;
    const CodecID resourceVideoStreamCodecID = CODEC_ID_H264;   //TODO/HLS #ak get codec of resource video stream. For HLS h264 is OK
    QSize videoResolution = QSize( 1280, 720 );  //TODO/HLS #ak get resolution of resource video stream. This resolution is ignored when TM_DirectStreamCopy is used
    if( transcodeParams.videoCodec().isEmpty() && !transcodeParams.pictureSizePixels().isValid() )
    {
        codecID = resourceVideoStreamCodecID;
        transcodeMethod = QnTranscoder::TM_DirectStreamCopy;
    }
    else
    {
        //AVOutputFormat* requestedVideoFormat = av_guess_format( transcodeParams.videoCodec().toLatin1().data(), NULL, NULL );
        codecID = 
            transcodeParams.videoCodec().isEmpty()
            ? resourceVideoStreamCodecID
            : av_guess_codec( NULL, transcodeParams.videoCodec().toLatin1().data(), NULL, NULL, AVMEDIA_TYPE_VIDEO );
        if( codecID == CODEC_ID_NONE )
        {
            NX_LOG( QString::fromLatin1("Cannot start transcoding of streaming chunk of resource %1. No codec %2 found in FFMPEG library").
                arg(mediaResource->toResource()->getUniqueId()).arg(transcodeParams.videoCodec()), cl_logWARNING );
            return false;
        }
        transcodeMethod = codecID == resourceVideoStreamCodecID ?
            QnTranscoder::TM_DirectStreamCopy :
            QnTranscoder::TM_FfmpegTranscode;
        if( transcodeParams.pictureSizePixels().isValid() )
            videoResolution = transcodeParams.pictureSizePixels();
    }
    if( transcoder->setVideoCodec( codecID, transcodeMethod, Qn::QualityNormal, videoResolution ) != 0 )
    {
        NX_LOG( QString::fromLatin1("Failed to create transcoder with video codec \"%1\" to transcode chunk (%2 - %3) of resource %4").
            arg(transcodeParams.videoCodec()).arg(transcodeParams.startTimestamp()).
            arg(transcodeParams.endTimestamp()).arg(transcodeParams.srcResourceUniqueID()), cl_logWARNING );
        return false;
    }

    //TODO/HLS #ak audio
    if( !transcodeParams.audioCodec().isEmpty() )
    {
        //if( transcoder->setAudioCodec( CODEC_ID_AAC, QnTranscoder::TM_FfmpegTranscode ) != 0 )
        //{
        //    NX_LOG( QString::fromLatin1("Failed to create transcoder with audio codec \"%1\" to transcode chunk (%2 - %3) of resource %4").
        //        arg(transcodeParams.audioCodec()).arg(transcodeParams.startTimestamp()).
        //        arg(transcodeParams.endTimestamp()).arg(transcodeParams.srcResourceUniqueID()), cl_logWARNING );
        //    return false;
        //}
    }

    //TODO/HLS #ak selecting least used transcoding thread from pool
    StreamingChunkTranscoderThread* transcoderThread = m_transcodeThreads[rand() % m_transcodeThreads.size()];

    //adding transcoder to transcoding thread
    transcoderThread->startTranscoding(
        transcodingID,
        chunk,
        AbstractOnDemandDataProviderPtr( new H264Mp4ToAnnexB( dataSource ) ),
        transcodeParams,
        transcoder.release() );

    return true;
}

bool StreamingChunkTranscoder::scheduleTranscoding(
    const int transcodeID,
    int delaySec )
{
    const quint64 taskID = TimerManager::instance()->addTimer(
        this,
        delaySec );

    QMutexLocker lk( &m_mutex );
    m_taskIDToTranscode[taskID] = transcodeID;
    return true;
}

bool StreamingChunkTranscoder::validateTranscodingParameters( const StreamingChunkCacheKey& /*transcodeParams*/ )
{
    //TODO/HLS #ak
    return true;
}
