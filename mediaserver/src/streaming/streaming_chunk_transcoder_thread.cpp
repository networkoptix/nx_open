////////////////////////////////////////////////////////////
// 14 jan 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "streaming_chunk_transcoder_thread.h"

#include <fstream>

#include <QMutexLocker>

#include <transcoding/ffmpeg_transcoder.h>
#include <utils/common/log.h>

#ifdef _DEBUG
//#define SAVE_INPUT_STREAM_TO_FILE
//#define SAVE_OUTPUT_TO_FILE
#endif


static const size_t RESERVED_TRANSCODED_PACKET_SIZE = 4096;
static const qint64 EMPTY_DATA_SOURCE_REREAD_TIMEOUT_MS = 1000;
static const size_t USEC_IN_MSEC = 1000;

using namespace std;

StreamingChunkTranscoderThread::TranscodeContext::TranscodeContext()
:
    chunk( nullptr ),
    dataAvailable( true ),
    prevReadTryTimestamp( -1 ),
    msTranscoded( 0 ),
    packetsTranscoded( 0 ),
    prevPacketTimestamp( -1 )
{
}

StreamingChunkTranscoderThread::TranscodeContext::TranscodeContext(
    StreamingChunkPtr const _chunk,
    DataSourceContextPtr _dataSourceCtx,
    const StreamingChunkCacheKey& _transcodeParams )
:
    chunk( _chunk ),
    dataSourceCtx( _dataSourceCtx ),
    transcodeParams( _transcodeParams ),
    dataAvailable( true ),
    prevReadTryTimestamp( -1 ),
    msTranscoded( 0 ),
    packetsTranscoded( 0 ),
    prevPacketTimestamp( -1 )
{
}

StreamingChunkTranscoderThread::StreamingChunkTranscoderThread()
{
    setObjectName( QLatin1String("StreamingChunkTranscoderThread") );
    m_monotonicClock.restart();
}

StreamingChunkTranscoderThread::~StreamingChunkTranscoderThread()
{
    std::for_each( m_transcodeContext.cbegin(), m_transcodeContext.cend(),
        []( const std::pair<int, TranscodeContext*>& p ){ delete p.second; } );
    m_transcodeContext.clear();
}

bool StreamingChunkTranscoderThread::startTranscoding(
    int transcodingID,
    StreamingChunkPtr const chunk,
    DataSourceContextPtr dataSourceCtx,
    const StreamingChunkCacheKey& transcodeParams )
{
    QMutexLocker lk( &m_mutex );

    //checking transcodingID uniqueness
    pair<map<int, TranscodeContext*>::iterator, bool>
        p = m_transcodeContext.insert( make_pair( transcodingID, (TranscodeContext*)nullptr ) );
    if( !p.second )
        return false;
    p.first->second = new TranscodeContext(
        chunk,
        dataSourceCtx,
        transcodeParams );
    m_dataSourceToID.insert( make_pair( dataSourceCtx->mediaDataProvider.get(), transcodingID ) );

#ifdef SAVE_OUTPUT_TO_FILE
    p.first->second->outputFile.setFileName( lit("c:\\tmp\\%1.ts").arg(transcodingID) );
    p.first->second->outputFile.open( QIODevice::WriteOnly );
#endif

    connect(
        dataSourceCtx->mediaDataProvider.get(), &AbstractOnDemandDataProvider::dataAvailable,
        this, &StreamingChunkTranscoderThread::onStreamDataAvailable,
        Qt::DirectConnection );

    m_cond.wakeAll();
    return true;
}

size_t StreamingChunkTranscoderThread::ongoingTranscodings() const
{
    QMutexLocker lk( &m_mutex );
    return m_transcodeContext.size();
}

//void StreamingChunkTranscoderThread::cancel( int transcodingID )
//{
//    QMutexLocker lk( &m_mutex );
//
//    map<int, TranscodeContext*>::iterator it = m_transcodeContext.find( transcodingID );
//    if( it == m_transcodeContext.end() )
//        return;
//    disconnect( it->second->dataSource.data(), 0, this, 0 );
//    removeTranscodingNonSafe( it, true, &lk );
//}

void StreamingChunkTranscoderThread::pleaseStop()
{
    QMutexLocker lk( &m_mutex );
    QnLongRunnable::pleaseStop();
    m_cond.wakeAll();
}

void StreamingChunkTranscoderThread::run()
{
    NX_LOG( QLatin1String("StreamingChunkTranscoderThread started"), cl_logDEBUG1 );

#ifdef SAVE_INPUT_STREAM_TO_FILE
    std::ofstream m_inputFile( "c:\\Temp\\chunk.264", std::ios_base::binary );
#endif

    int prevReadTranscodingID = 0;
    while( !needToStop() )
    {
        QMutexLocker lk( &m_mutex );

        const qint64 currentMonotonicTimestamp = m_monotonicClock.elapsed();
        //taking context with data - trying to find context different from previous one
        std::map<int, TranscodeContext*>::iterator transcodeIter = m_transcodeContext.upper_bound( prevReadTranscodingID );
        for( size_t i = 0; i < m_transcodeContext.size(); ++i )
        {
            if( transcodeIter == m_transcodeContext.end() )
                transcodeIter = m_transcodeContext.begin();
            if( transcodeIter->second->prevReadTryTimestamp + EMPTY_DATA_SOURCE_REREAD_TIMEOUT_MS < currentMonotonicTimestamp )
                transcodeIter->second->dataAvailable = true;    //retrying data source
            if( transcodeIter->second->dataAvailable )
                break;      //TODO/HLS #ak: using dataAvailable looks unreliable, since logic based on AbstractOnDemandDataProvider::dataAvailable is event-triggered
                                //but AbstractOnDemandDataProvider::tryRead is level-triggered, which is better
            ++transcodeIter;
        }

        if( transcodeIter == m_transcodeContext.end() || !transcodeIter->second->dataAvailable )
        {
            //nothing to do
            m_cond.wait( lk.mutex(), EMPTY_DATA_SOURCE_REREAD_TIMEOUT_MS ); //TODO #ak this timeout is not correct, have to find min timeout to 
            continue;
        }

        prevReadTranscodingID = transcodeIter->first;

        //performing transcode
        QnAbstractDataPacketPtr srcPacket;
        if( !transcodeIter->second->dataSourceCtx->mediaDataProvider->tryRead( &srcPacket ) )
        {
            transcodeIter->second->prevReadTryTimestamp = currentMonotonicTimestamp;
            transcodeIter->second->dataAvailable = false;
            continue;
        }

        if( !srcPacket )
        {
            NX_LOG( QString::fromLatin1("End of file reached while transcoding resource %1 data. Transcoded %2 ms of source data").
                arg(transcodeIter->second->transcodeParams.srcResourceUniqueID()).arg(transcodeIter->second->msTranscoded), cl_logDEBUG1 );
            removeTranscodingNonSafe( transcodeIter, false, &lk );
            continue;
        }

        QnAbstractMediaDataPtr srcMediaData = srcPacket.dynamicCast<QnAbstractMediaData>();
        Q_ASSERT( srcMediaData );

        QnByteArray resultStream( 1, RESERVED_TRANSCODED_PACKET_SIZE );
#ifdef SAVE_INPUT_STREAM_TO_FILE
        m_inputFile.write( srcMediaData->data.constData(), srcMediaData->data.size() );
#endif
        int res = transcodeIter->second->dataSourceCtx->transcoder->transcodePacket( srcMediaData, &resultStream );
        if( res )
        {
            NX_LOG( QString::fromLatin1("Error transcoding resource %1 data, error code %2. Transcoded %3 ms of source data").
                arg(transcodeIter->second->transcodeParams.srcResourceUniqueID()).arg(res).arg(transcodeIter->second->msTranscoded), cl_logWARNING );
            removeTranscodingNonSafe( transcodeIter, false, &lk );
            continue;
        }

        ++transcodeIter->second->packetsTranscoded;

        //protecting from discontinuity in timestamp
        if( transcodeIter->second->prevPacketTimestamp == -1 )
            transcodeIter->second->prevPacketTimestamp = srcMediaData->timestamp;
        if( qAbs(srcMediaData->timestamp - transcodeIter->second->prevPacketTimestamp) > 3600*1000*1000LL ) //1 hour (usec)
            transcodeIter->second->prevPacketTimestamp = srcMediaData->timestamp;   //discontinuity
        if( srcMediaData->timestamp > transcodeIter->second->prevPacketTimestamp )
        {
            transcodeIter->second->msTranscoded += (srcMediaData->timestamp - transcodeIter->second->prevPacketTimestamp) / 1000;
            transcodeIter->second->prevPacketTimestamp = srcMediaData->timestamp;
        }

        //adding data to chunk
        if( resultStream.size() > 0 )
        {
            transcodeIter->second->chunk->appendData( QByteArray::fromRawData(resultStream.constData(), resultStream.size()) );
#ifdef SAVE_OUTPUT_TO_FILE
            transcodeIter->second->outputFile.write( QByteArray::fromRawData(resultStream.constData(), resultStream.size()) );
#endif
        }

        //TODO #ak support opened ending (when transcodeParams.endTimestamp() is known only after transcoding is done), 
            //that required for minimizing hls live delay 
        if( transcodeIter->second->dataSourceCtx->mediaDataProvider->currentPos() >= transcodeIter->second->transcodeParams.endTimestamp() )
            //|| transcodeIter->second->msTranscoded * USEC_IN_MSEC >= transcodeIter->second->transcodeParams.duration() )
        {
#ifdef SAVE_INPUT_STREAM_TO_FILE
            m_inputFile.close();
#endif
#ifdef SAVE_OUTPUT_TO_FILE
            transcodeIter->second->outputFile.close();
#endif
            //neccessary source data has been processed, depleting transcoder buffer
            for( ;; )
            {
                resultStream.clear();
                int res = transcodeIter->second->dataSourceCtx->transcoder->transcodePacket( QnAbstractMediaDataPtr( new QnEmptyMediaData() ), &resultStream );
                if( res || resultStream.size() == 0 )
                    break;
                transcodeIter->second->chunk->appendData( QByteArray::fromRawData(resultStream.constData(), resultStream.size()) );
            }

            //removing transcoding
            removeTranscodingNonSafe( transcodeIter, true, &lk );
        }
    }

    NX_LOG( QLatin1String("StreamingChunkTranscoderThread stopped"), cl_logDEBUG1 );
}

void StreamingChunkTranscoderThread::onStreamDataAvailable( AbstractOnDemandDataProvider* dataSource )
{
    QMutexLocker lk( &m_mutex );

    //locating transcoding context by data source
    std::map<AbstractOnDemandDataProvider*, int>::const_iterator
        dsIter = m_dataSourceToID.find( dataSource );
    if( dsIter == m_dataSourceToID.end() )
        return; //this is possible just after transcoding removal

    //marking, that data is available
    map<int, TranscodeContext*>::const_iterator it = m_transcodeContext.find( dsIter->second );
    Q_ASSERT( it != m_transcodeContext.end() );
    it->second->dataAvailable = true;

    m_cond.wakeAll();
}

void StreamingChunkTranscoderThread::removeTranscodingNonSafe(
    const std::map<int, TranscodeContext*>::iterator& transcodingIter,
    bool transcodingFinishedSuccessfully,
    QMutexLocker* const lk )
{
    StreamingChunkPtr chunk = transcodingIter->second->chunk;

    m_dataSourceToID.erase( transcodingIter->second->dataSourceCtx->mediaDataProvider.get() );
    TranscodeContext* tc = transcodingIter->second;
    const int transcodingID = transcodingIter->first;
    m_transcodeContext.erase( transcodingIter );

    lk->unlock();
    chunk->doneModification( transcodingFinishedSuccessfully ? StreamingChunk::rcEndOfData : StreamingChunk::rcError );
    emit transcodingFinished(
        transcodingID,
        transcodingFinishedSuccessfully,
        tc->transcodeParams,
        tc->dataSourceCtx );
    delete tc;
    lk->relock();
}
