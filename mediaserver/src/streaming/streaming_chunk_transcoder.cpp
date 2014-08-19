////////////////////////////////////////////////////////////
// 18 dec 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "streaming_chunk_transcoder.h"

#include <QtCore/QMutexLocker>

#include <core/dataprovider/h264_mp4_to_annexb.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/security_cam_resource.h>
#include <plugins/resource/archive/abstract_archive_stream_reader.h>
#include <recording/time_period.h>
#include <transcoding/ffmpeg_transcoder.h>
#include <utils/common/log.h>
#include <utils/common/timermanager.h>

#include "ondemand_media_data_provider.h"
#include "live_media_cache_reader.h"
#include "streaming_chunk_cache_key.h"
#include "streaming_chunk_transcoder_thread.h"
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
    m_terminated( false ),
    m_flags( flags ),
    m_transcodeIDSeq( 1 )
{
    m_transcodeThreads.resize( TRANSCODE_THREAD_COUNT );
    for( size_t i = 0; i < m_transcodeThreads.size(); ++i )
    {
        m_transcodeThreads[i] = new StreamingChunkTranscoderThread();
        connect(
            m_transcodeThreads[i], &StreamingChunkTranscoderThread::transcodingFinished,
            this, &StreamingChunkTranscoder::onTranscodingFinished,
            Qt::DirectConnection );
        m_transcodeThreads[i]->start();
    }
}

StreamingChunkTranscoder::~StreamingChunkTranscoder()
{
    //cancelling all scheduled transcodings
    {
        QMutexLocker lk( &m_mutex );
        m_terminated = true;
    }

    for( auto val: m_taskIDToTranscode )
        TimerManager::instance()->joinAndDeleteTimer( val.first );

    std::for_each( m_transcodeThreads.begin(), m_transcodeThreads.end(), std::default_delete<StreamingChunkTranscoderThread>() );
    m_transcodeThreads.clear();
}

bool StreamingChunkTranscoder::transcodeAsync(
    const StreamingChunkCacheKey& transcodeParams,
    StreamingChunkPtr chunk )
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

    const int newTranscodingID = m_transcodeIDSeq.fetchAndAddAcquire(1);

    chunk->openForModification();

    DataSourceContextPtr dataSourceCtx = m_dataSourceCache.find( transcodeParams );
    if( dataSourceCtx )
    {
        NX_LOG( lit("Taking reader for resource %1, start timestamp %2, duration %3 from cache").
            arg(transcodeParams.srcResourceUniqueID()).arg(transcodeParams.startTimestamp()).arg(transcodeParams.duration()), cl_logDEBUG2 );
    }
    else
    {
        AbstractOnDemandDataProviderPtr mediaDataProvider;
        //checking requested time region: 
            //whether data is present (in archive or cache)
        if( transcodeParams.live() )
        {
            if( !camera->liveCache(transcodeParams.streamQuality()) )
            {
                chunk->doneModification( StreamingChunk::rcError );
                return false;
            }

            NX_LOG( lit("Creating LIVE reader for resource %1, start timestamp %2, duration %3").
                arg(transcodeParams.srcResourceUniqueID()).arg(transcodeParams.startTimestamp()).arg(transcodeParams.duration()), cl_logDEBUG2 );

            const quint64 cacheStartTimestamp = camera->liveCache(transcodeParams.streamQuality())->startTimestamp();
            const quint64 cacheEndTimestamp = camera->liveCache(transcodeParams.streamQuality())->currentTimestamp();
            const quint64 actualStartTimestamp = std::max<>( cacheStartTimestamp, transcodeParams.startTimestamp() );
            mediaDataProvider = AbstractOnDemandDataProviderPtr( new LiveMediaCacheReader( camera->liveCache(transcodeParams.streamQuality()), actualStartTimestamp ) );

            if( (transcodeParams.startTimestamp() < cacheEndTimestamp &&     //requested data is in live cache (at least, partially)
                 transcodeParams.endTimestamp() > cacheStartTimestamp) ||
                (transcodeParams.startTimestamp() > cacheEndTimestamp &&     //chunk is in future not futher, than MAX_CHUNK_TIMESTAMP_ADVANCE_MICROS
                 transcodeParams.startTimestamp() - cacheEndTimestamp < MAX_CHUNK_TIMESTAMP_ADVANCE_MICROS) ||
                !transcodeParams.alias().isEmpty() )    //have alias, startTimestamp may be invalid
            {
            }
            else
            {
                //no data. e.g., request is too far away in future
                chunk->doneModification( StreamingChunk::rcError );
                return false;
            }
        }
        else
        {
            NX_LOG( lit("Creating archive reader for resource %1, start timestamp %2, duration %3").
                arg(transcodeParams.srcResourceUniqueID()).arg(transcodeParams.startTimestamp()).arg(transcodeParams.duration()), cl_logDEBUG2 );

            //creating archive reader
            QSharedPointer<QnAbstractStreamDataProvider> dp( cameraResource->createDataProvider( Qn::CR_Archive ) );
            if( !dp )
            {
                NX_LOG( lit("StreamingChunkTranscoder::transcodeAsync. Failed (1) to create archive data provider (resource %1)").
                    arg(transcodeParams.srcResourceUniqueID()), cl_logWARNING );
                chunk->doneModification( StreamingChunk::rcError );
                return false;
            }

            QnAbstractArchiveReader* archiveReader = dynamic_cast<QnAbstractArchiveReader*>(dp.data());
            if( !archiveReader || !archiveReader->open() )
            {
                NX_LOG( lit("StreamingChunkTranscoder::transcodeAsync. Failed (2) to create archive data provider (resource %1)").
                    arg(transcodeParams.srcResourceUniqueID()), cl_logWARNING );
                chunk->doneModification( StreamingChunk::rcError );
                return false;
            }
            archiveReader->setQuality(transcodeParams.streamQuality(), true);
            archiveReader->setPlaybackRange( QnTimePeriod( transcodeParams.startTimestamp() / USEC_IN_MSEC, transcodeParams.duration() ) );
            mediaDataProvider = OnDemandMediaDataProviderPtr( new OnDemandMediaDataProvider( dp ) );
            archiveReader->start();
        }

        mediaDataProvider = AbstractOnDemandDataProviderPtr( new H264Mp4ToAnnexB( mediaDataProvider ) );    
            //TODO #ak this is not always required, and should be done in createTranscoder method

        //creating transcoder
        QnTranscoderPtr transcoder( createTranscoder( cameraResource, transcodeParams ) );
        if( !transcoder )
        {
            NX_LOG( lit("StreamingChunkTranscoder::transcodeAsync. Failed to create transcoder. resource %1, format %2, video codec %3").
                arg(transcodeParams.srcResourceUniqueID()).arg(transcodeParams.containerFormat()).arg(transcodeParams.videoCodec()), cl_logWARNING );
            chunk->doneModification( StreamingChunk::rcError );
            return false;
        }

        dataSourceCtx = DataSourceContextPtr( new DataSourceContext( mediaDataProvider, transcoder ) );
    }

    if( transcodeParams.live() )
    {
        NX_LOG( lit("Starting transcoding startTimestamp %1, duration %2").arg(transcodeParams.startTimestamp()).arg(transcodeParams.duration()), cl_logDEBUG1 );

        const quint64 cacheEndTimestamp = camera->liveCache(transcodeParams.streamQuality())->currentTimestamp();
        if( transcodeParams.alias().isEmpty() &&
            transcodeParams.startTimestamp() > cacheEndTimestamp &&
            transcodeParams.startTimestamp() - cacheEndTimestamp < MAX_CHUNK_TIMESTAMP_ADVANCE_MICROS )
        {
            //chunk is in future not futher, than MAX_CHUNK_TIMESTAMP_ADVANCE_MICROS, scheduling transcoding on data availability
            pair<map<int, TranscodeContext>::iterator, bool> p = m_scheduledTranscodings.insert( make_pair( newTranscodingID, TranscodeContext() ) );
            Q_ASSERT( p.second );

            p.first->second.mediaResource = cameraResource;
            p.first->second.dataSourceCtx = dataSourceCtx;
            p.first->second.transcodeParams = transcodeParams;
            p.first->second.chunk = chunk;

            if( scheduleTranscoding(
                    newTranscodingID,
                    (transcodeParams.startTimestamp() - cacheEndTimestamp) / USEC_IN_MSEC + 1 ) )
            {
                return true;
            }
            chunk->doneModification( StreamingChunk::rcError );
            m_scheduledTranscodings.erase( p.first );
            return false;
        }
    }

    if( !startTranscoding(
            newTranscodingID,
            dataSourceCtx,
            transcodeParams,
            chunk ) )
    {
        chunk->doneModification( StreamingChunk::rcError );
        return false;
    }

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

    if( m_terminated )
        return;

    std::map<quint64, int>::const_iterator taskIter = m_taskIDToTranscode.find( timerID );
    if( taskIter == m_taskIDToTranscode.end() )
    {
        NX_LOG( QString("StreamingChunkTranscoder::onTimer. Received timer event with unknown id %1. Ignoring...").arg(timerID), cl_logDEBUG1 );
        return;
    }

    const int transcodeID = taskIter->second;
    m_taskIDToTranscode.erase( taskIter );

    std::map<int, TranscodeContext>::iterator transcodeIter = m_scheduledTranscodings.find( transcodeID );
    if( transcodeIter == m_scheduledTranscodings.end() )
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
            transcodeIter->second.dataSourceCtx,
            transcodeIter->second.transcodeParams,
            transcodeIter->second.chunk ) )
    {
        NX_LOG( QString::fromLatin1("StreamingChunkTranscoder::onTimer. Failed to start transcoding (resource %1)").
            arg(transcodeIter->second.mediaResource->toResource()->getUniqueId()), cl_logWARNING );
        transcodeIter->second.chunk->doneModification( StreamingChunk::rcError );
    }

    m_scheduledTranscodings.erase( transcodeIter );
}

bool StreamingChunkTranscoder::startTranscoding(
    int transcodingID,
    DataSourceContextPtr dataSourceCtx,
    const StreamingChunkCacheKey& transcodeParams,
    StreamingChunkPtr chunk )
{
    //selecting least used transcoding thread from pool
    StreamingChunkTranscoderThread* transcoderThread = *std::max_element( m_transcodeThreads.cbegin(), m_transcodeThreads.cend(), 
        [](StreamingChunkTranscoderThread* one, StreamingChunkTranscoderThread* two){
            return one->ongoingTranscodings() < two->ongoingTranscodings();
        } );

    //adding transcoder to transcoding thread
    return transcoderThread->startTranscoding(
        transcodingID,
        chunk,
        dataSourceCtx,
        transcodeParams );
}

bool StreamingChunkTranscoder::scheduleTranscoding(
    const int transcodeID,
    int delayMSec )
{
    const quint64 taskID = TimerManager::instance()->addTimer(
        this,
        delayMSec );

    QMutexLocker lk( &m_mutex );
    m_taskIDToTranscode[taskID] = transcodeID;
    return true;
}

bool StreamingChunkTranscoder::validateTranscodingParameters( const StreamingChunkCacheKey& /*transcodeParams*/ )
{
    //TODO #ak
    return true;
}

QnTranscoder* StreamingChunkTranscoder::createTranscoder(
    const QnSecurityCamResourcePtr& mediaResource,
    const StreamingChunkCacheKey& transcodeParams )
{
    //launching transcoding:
        //creating transcoder
    std::unique_ptr<QnFfmpegTranscoder> transcoder( new QnFfmpegTranscoder() );
    if( transcoder->setContainer( transcodeParams.containerFormat() ) != 0 )
    {
        NX_LOG( QString::fromLatin1("Failed to create transcoder with container \"%1\" to transcode chunk (%2 - %3) of resource %4").
            arg(transcodeParams.containerFormat()).arg(transcodeParams.startTimestamp()).
            arg(transcodeParams.endTimestamp()).arg(transcodeParams.srcResourceUniqueID()), cl_logWARNING );
        return nullptr;
    }
    CodecID codecID = CODEC_ID_NONE;
    QnTranscoder::TranscodeMethod transcodeMethod = QnTranscoder::TM_DirectStreamCopy;
    const CodecID resourceVideoStreamCodecID = CODEC_ID_H264;   //TODO #ak: get codec of resource video stream. Currently (only HLS uses this class), it is always h.264
    QSize videoResolution;
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
            return nullptr;
        }
        transcodeMethod = codecID == resourceVideoStreamCodecID ?   //TODO: #ak and resolusion did not change
            QnTranscoder::TM_DirectStreamCopy :
            QnTranscoder::TM_FfmpegTranscode;
        if( transcodeParams.pictureSizePixels().isValid() )
        {
            videoResolution = transcodeParams.pictureSizePixels();
        }
        else
        {
            assert( false );
            videoResolution = QSize( 1280, 720 );  //TODO: #ak get resolution of resource video stream. This resolution is ignored when TM_DirectStreamCopy is used
        }
    }
    if( transcoder->setVideoCodec( codecID, transcodeMethod, Qn::QualityNormal, videoResolution ) != 0 )
    {
        NX_LOG( QString::fromLatin1("Failed to create transcoder with video codec \"%1\" to transcode chunk (%2 - %3) of resource %4").
            arg(transcodeParams.videoCodec()).arg(transcodeParams.startTimestamp()).
            arg(transcodeParams.endTimestamp()).arg(transcodeParams.srcResourceUniqueID()), cl_logWARNING );
        return nullptr;
    }

    //TODO/HLS #ak audio
    if( !transcodeParams.audioCodec().isEmpty() )
    {
        //if( transcoder->setAudioCodec( CODEC_ID_AAC, QnTranscoder::TM_FfmpegTranscode ) != 0 )
        //{
        //    NX_LOG( QString::fromLatin1("Failed to create transcoder with audio codec \"%1\" to transcode chunk (%2 - %3) of resource %4").
        //        arg(transcodeParams.audioCodec()).arg(transcodeParams.startTimestamp()).
        //        arg(transcodeParams.endTimestamp()).arg(transcodeParams.srcResourceUniqueID()), cl_logWARNING );
        //    return nullptr;
        //}
    }

    return transcoder.release();
}

void StreamingChunkTranscoder::onTranscodingFinished(
    int /*transcodingID*/,
    bool result,
    const StreamingChunkCacheKey& key,
    DataSourceContextPtr data )
{
    if( !result )
    {
        //TODO/HLS #ak: MUST remove chunk from cache
        return;
    }
    m_dataSourceCache.put( key, data, (key.duration() / USEC_IN_MSEC) * 3 );  //ideally, <max chunk length from previous playlist> * <chunk count in playlist>
}
