////////////////////////////////////////////////////////////
// 14 jan 2012    Andrey Kolesnikov
////////////////////////////////////////////////////////////

#include "streaming_chunk_transcoder_thread.h"

#include <QMutexLocker>

#include <transcoding/ffmpeg_transcoder.h>
#include <utils/common/log.h>

#include "streaming_chunk.h"


using namespace std;

StreamingChunkTranscoderThread::TranscodeContext::TranscodeContext()
:
    chunk( NULL ),
    transcoder( 0 ),
    dataAvailable( false ),
    msTranscoded( 0 ),
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
    transcoder( _transcoder )
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
        dataSource.data(), SIGNAL(dataAvailable(AbstractOnDemandDataProvider*)),
        this, SLOT(onStreamDataAvailable(AbstractOnDemandDataProvider*)),
        Qt::DirectConnection );

    m_cond.wakeAll();
    return true;
}

void StreamingChunkTranscoderThread::cancel( int transcodingID )
{
    QMutexLocker lk( &m_mutex );

    map<int, TranscodeContext*>::iterator it = m_transcodeContext.find( transcodingID );
    if( it == m_transcodeContext.end() )
        return;
    disconnect( it->second->dataSource.data(), 0, this, 0 );
    removeTranscodingNonSafe( it, true );
}

void StreamingChunkTranscoderThread::pleaseStop()
{
    QMutexLocker lk( &m_mutex );
    QnLongRunnable::pleaseStop();
    m_cond.wakeAll();
}

void StreamingChunkTranscoderThread::run()
{
    NX_LOG( QLatin1String("StreamingChunkTranscoderThread started"), cl_logDEBUG1 );

    int prevReadTranscodingID = 0;
    while( !needToStop() )
    {
        QMutexLocker lk( &m_mutex );

        //taking context with data - trying to find context different from previous one
        std::map<int, TranscodeContext*>::iterator transcodeIter = m_transcodeContext.upper_bound( prevReadTranscodingID );
        for( int i = 0; i < m_transcodeContext.size(); ++i )
        {
            if( transcodeIter == m_transcodeContext.end() )
                transcodeIter = m_transcodeContext.begin();
            if( transcodeIter->second->dataAvailable )
                break;
            ++transcodeIter;
        }

        if( transcodeIter == m_transcodeContext.end() || !transcodeIter->second->dataAvailable )
        {
            //nothing to do
            m_cond.wait( lk.mutex() );
            continue;
        }

        prevReadTranscodingID = transcodeIter->first;

        //performing transcode
        QnAbstractDataPacketPtr srcPacket;
        if( !transcodeIter->second->dataSource->tryRead( &srcPacket ) )
            continue;

        if( !srcPacket )
        {
            NX_LOG( QString::fromLatin1("End of file reached while transcoding resource %1 data. Transcoded %2 ms of source data").
                arg(transcodeIter->second->transcodeParams.srcResourceUniqueID()).arg(transcodeIter->second->msTranscoded), cl_logDEBUG1 );
            //TODO/IMPL/HLS remove transcoding or consider that data may appear later?
        }

        QnAbstractMediaDataPtr srcMediaData = srcPacket.dynamicCast<QnAbstractMediaData>();
        Q_ASSERT( srcMediaData );

        QnByteArray resultStream( 1, 1024 );
        //TODO/IMPL/HLS releasing mutex lock for transcodePacket call
        int res = transcodeIter->second->transcoder->transcodePacket( srcMediaData, resultStream );
        if( res )
        {
            NX_LOG( QString::fromLatin1("Error transcoding resource %1 data, error code %2. Transcoded %3 ms of source data").
                arg(transcodeIter->second->transcodeParams.srcResourceUniqueID()).arg(res).arg(transcodeIter->second->msTranscoded), cl_logWARNING );
            removeTranscodingNonSafe( transcodeIter, false );
            continue;
        }

        //TODO/IMPL/HLS protect from discontinuity in timestamp
        if( transcodeIter->second->prevPacketTimestamp == -1 )
            transcodeIter->second->prevPacketTimestamp = srcMediaData->timestamp;
        if( qAbs(srcMediaData->timestamp - transcodeIter->second->prevPacketTimestamp) > 3600*1000*1000 )
            transcodeIter->second->prevPacketTimestamp = srcMediaData->timestamp;   //discontinuity
        if( srcMediaData->timestamp > transcodeIter->second->prevPacketTimestamp )
        {
            transcodeIter->second->msTranscoded += (srcMediaData->timestamp - transcodeIter->second->prevPacketTimestamp) / 1000;
            transcodeIter->second->prevPacketTimestamp = srcMediaData->timestamp;
        }

        //adding data to chunk
        if( resultStream.size() > 0 )
            transcodeIter->second->chunk->appendData( QByteArray::fromRawData(resultStream.constData(), resultStream.size()) );

        //if( transcodeIter->second->msTranscoded >= transcodeIter->second->transcodeParams.duration() )
        if( transcodeIter->second->dataSource->currentPos() >= transcodeIter->second->transcodeParams.endTimestamp() )
        {
            //neccessary source data has been processed, depleting transcoder buffer
            for( ;; )
            {
                resultStream.clear();
                int res = transcodeIter->second->transcoder->transcodePacket( QnAbstractMediaDataPtr( new QnEmptyMediaData() ), resultStream );
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
        //TODO/IMPL/HLS MUST remove chunk from cache
    }

    m_dataSourceToID.erase( transcodingIter->second->dataSource.data() );
    m_transcodeContext.erase( transcodingIter );

    //TODO/IMPL/HLS emit some signal?
}
