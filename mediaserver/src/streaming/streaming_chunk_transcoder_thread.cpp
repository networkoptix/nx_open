////////////////////////////////////////////////////////////
// 14 jan 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "streaming_chunk_transcoder_thread.h"

#include <fstream>

#include <QMutexLocker>

#include <transcoding/ffmpeg_transcoder.h>
#include <utils/common/log.h>

#include "streaming_chunk.h"

#ifdef _DEBUG
//#define SAVE_INPUT_STREAM_TO_FILE
#endif


static const int RESERVED_TRANSCODED_PACKET_SIZE = 4096;

using namespace std;

StreamingChunkTranscoderThread::TranscodeContext::TranscodeContext()
:
    chunk( NULL ),
    transcoder( NULL ),
    dataAvailable( true ),
    msTranscoded( 0 ),
    packetsTranscoded( 0 ),
    prevPacketTimestamp( -1 )
{
}

StreamingChunkTranscoderThread::TranscodeContext::TranscodeContext(
    StreamingChunk* const _chunk,
    QSharedPointer<AbstractOnDemandDataProvider> _dataSource,
    const StreamingChunkCacheKey& _transcodeParams,
    QnFfmpegTranscoder* _transcoder )
:
    chunk( _chunk ),
    dataSource( _dataSource ),
    transcodeParams( _transcodeParams ),
    transcoder( _transcoder ),
    dataAvailable( true ),
    msTranscoded( 0 ),
    packetsTranscoded( 0 ),
    prevPacketTimestamp( -1 )
{
}

StreamingChunkTranscoderThread::StreamingChunkTranscoderThread()
{
    setObjectName( QLatin1String("StreamingChunkTranscoderThread") );
}

StreamingChunkTranscoderThread::~StreamingChunkTranscoderThread()
{
    for( std::map<int, TranscodeContext*>::const_iterator
        it = m_transcodeContext.begin();
        it != m_transcodeContext.end();
        ++it )
    {
        delete it->second;
    }
    m_transcodeContext.clear();
}

bool StreamingChunkTranscoderThread::startTranscoding(
    int transcodingID,
    StreamingChunk* const chunk,
    QSharedPointer<AbstractOnDemandDataProvider> dataSource,
    const StreamingChunkCacheKey& transcodeParams,
    QnFfmpegTranscoder* transcoder )
{
    QMutexLocker lk( &m_mutex );

    //checking transcodingID uniqueness
    pair<map<int, TranscodeContext*>::iterator, bool>
        p = m_transcodeContext.insert( make_pair( transcodingID, (TranscodeContext*)NULL ) );
    if( !p.second )
        return false;
    p.first->second = new TranscodeContext(
        chunk,
        dataSource,
        transcodeParams,
        transcoder );
    m_dataSourceToID.insert( make_pair( dataSource.data(), transcodingID ) );

    connect(
        dataSource.data(), &AbstractOnDemandDataProvider::dataAvailable,
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
//    removeTranscodingNonSafe( it, true );
//}

void StreamingChunkTranscoderThread::pleaseStop()
{
    QMutexLocker lk( &m_mutex );
    QnLongRunnable::pleaseStop();
    m_cond.wakeAll();
}

static const int THREAD_STOP_CHECK_TIMEOUT_MS = 1000;

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

        //taking context with data - trying to find context different from previous one
        std::map<int, TranscodeContext*>::iterator transcodeIter = m_transcodeContext.upper_bound( prevReadTranscodingID );
        for( size_t i = 0; i < m_transcodeContext.size(); ++i )
        {
            if( transcodeIter == m_transcodeContext.end() )
                transcodeIter = m_transcodeContext.begin();
            if( transcodeIter->second->dataAvailable )
                break;      //TODO/HLS #ak: using dataAvailable looks unreliable, since logic based on AbstractOnDemandDataProvider::dataAvailable is event-triggered
                                //but AbstractOnDemandDataProvider::tryRead is level-triggered, which is better
            ++transcodeIter;
        }

        if( transcodeIter == m_transcodeContext.end() || !transcodeIter->second->dataAvailable )
        {
            //nothing to do
            m_cond.wait( lk.mutex(), THREAD_STOP_CHECK_TIMEOUT_MS );
            continue;
        }

        prevReadTranscodingID = transcodeIter->first;

        //performing transcode
        QnAbstractDataPacketPtr srcPacket;
        if( !transcodeIter->second->dataSource->tryRead( &srcPacket ) )
        {
            transcodeIter->second->dataAvailable = false;
            continue;
        }

        if( !srcPacket )
        {
            NX_LOG( QString::fromLatin1("End of file reached while transcoding resource %1 data. Transcoded %2 ms of source data").
                arg(transcodeIter->second->transcodeParams.srcResourceUniqueID()).arg(transcodeIter->second->msTranscoded), cl_logDEBUG1 );
            //TODO/HLS #ak: remove transcoding or consider that data may appear later?
        }

        QnAbstractMediaDataPtr srcMediaData = srcPacket.dynamicCast<QnAbstractMediaData>();
        Q_ASSERT( srcMediaData );

        QnByteArray resultStream( 1, RESERVED_TRANSCODED_PACKET_SIZE );
        //TODO/HLS #ak: releasing mutex lock for transcodePacket call
#ifdef SAVE_INPUT_STREAM_TO_FILE
        m_inputFile.write( srcMediaData->data.constData(), srcMediaData->data.size() );
#endif
        int res = transcodeIter->second->transcoder->transcodePacket( srcMediaData, &resultStream );
        if( res )
        {
            NX_LOG( QString::fromLatin1("Error transcoding resource %1 data, error code %2. Transcoded %3 ms of source data").
                arg(transcodeIter->second->transcodeParams.srcResourceUniqueID()).arg(res).arg(transcodeIter->second->msTranscoded), cl_logWARNING );
            removeTranscodingNonSafe( transcodeIter, false );
            continue;
        }

        ++transcodeIter->second->packetsTranscoded;

        //TODO/HLS #ak: protect from discontinuity in timestamp
        if( transcodeIter->second->prevPacketTimestamp == -1 )
            transcodeIter->second->prevPacketTimestamp = srcMediaData->timestamp;
        if( qAbs(srcMediaData->timestamp - transcodeIter->second->prevPacketTimestamp) > 3600*1000*1000UL )
            transcodeIter->second->prevPacketTimestamp = srcMediaData->timestamp;   //discontinuity
        if( srcMediaData->timestamp > transcodeIter->second->prevPacketTimestamp )
        {
            transcodeIter->second->msTranscoded += (srcMediaData->timestamp - transcodeIter->second->prevPacketTimestamp) / 1000;
            transcodeIter->second->prevPacketTimestamp = srcMediaData->timestamp;
        }

        //adding data to chunk
        if( resultStream.size() > 0 )
            transcodeIter->second->chunk->appendData( QByteArray::fromRawData(resultStream.constData(), resultStream.size()) );

        if( transcodeIter->second->dataSource->currentPos() >= transcodeIter->second->transcodeParams.endTimestamp() )
        {
#ifdef SAVE_INPUT_STREAM_TO_FILE
            m_inputFile.close();
#endif
            //neccessary source data has been processed, depleting transcoder buffer
            for( ;; )
            {
                resultStream.clear();
                int res = transcodeIter->second->transcoder->transcodePacket( QnAbstractMediaDataPtr( new QnEmptyMediaData() ), &resultStream );
                if( res || resultStream.size() == 0 )
                    break;
                transcodeIter->second->chunk->appendData( QByteArray::fromRawData(resultStream.constData(), resultStream.size()) );
            }

            //removing transcoding
            removeTranscodingNonSafe( transcodeIter, true );
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
    bool transcodingFinishedSuccessfully )
{
    transcodingIter->second->chunk->doneModification( transcodingFinishedSuccessfully ? StreamingChunk::rcEndOfData : StreamingChunk::rcError );

    if( !transcodingFinishedSuccessfully )
    {
        //TODO/HLS #ak: MUST remove chunk from cache
    }

    m_dataSourceToID.erase( transcodingIter->second->dataSource.data() );
    m_transcodeContext.erase( transcodingIter );

    //TODO/HLS #ak: emit some signal?
}
