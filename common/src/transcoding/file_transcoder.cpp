/**********************************************************
* 29 apr 2013
* a.kolesnikov
***********************************************************/

#include "file_transcoder.h"

#ifdef ENABLE_DATA_PROVIDERS

#include <memory>

#include <QtCore/QDir>
#include <nx/utils/thread/mutex.h>
#include <QtCore/QDir>
#include <QtCore/QFileInfo>

#include <core/resource/dummy_resource.h>
#include <utils/fs/file.h>
#include <nx/utils/random.h>

FileTranscoder::FileTranscoder()
:
    m_resultCode( 0 ),
    m_state( sInit ),
    m_transcodeDurationLimit( 0 ),
    m_transcodedDataDuration( 0 )
{
}

FileTranscoder::~FileTranscoder()
{
    pleaseStop();
    stop();
}

bool FileTranscoder::setSourceFile( const QString& filePath )
{
    m_mediaFileReader.reset( NULL );
    m_srcFilePath = filePath;
    return true;
}

bool FileTranscoder::setDestFile( const QString& filePath )
{
    m_dstFilePath = filePath;
    setDest( new QFile(filePath) );
    return true;
}

bool FileTranscoder::setContainer( const QString& containerName )
{
    return m_transcoder.setContainer( containerName ) == QnTranscoder::OperationResult::Success;
}

bool FileTranscoder::addTag( const QString& name, const QString& value )
{
    return m_transcoder.addTag( name, value );
}

bool FileTranscoder::setVideoCodec(
    AVCodecID codec,
    QnTranscoder::TranscodeMethod transcodeMethod,
    Qn::StreamQuality quality,
    const QSize& resolution,
    int bitrate,
    QnCodecParams::Value params )
{
    return m_transcoder.setVideoCodec( codec, transcodeMethod, quality, resolution, bitrate, params ) == QnTranscoder::OperationResult::Success;
}

bool FileTranscoder::setAudioCodec(
    AVCodecID codec,
    QnTranscoder::TranscodeMethod transcodeMethod )
{
    return m_transcoder.setAudioCodec( codec, transcodeMethod ) == QnTranscoder::OperationResult::Success;
}

void FileTranscoder::setTranscodeDurationLimit( unsigned int lengthToReadMS )
{
    m_transcodeDurationLimit = lengthToReadMS;
}

int FileTranscoder::resultCode() const
{
    return m_resultCode;
}

bool FileTranscoder::setTagValue(
    const QString& srcFilePath,
    const QString& name,
    const QString& value )
{
    AVFormatContext* formatCtx = NULL;
    if( avformat_open_input( &formatCtx, srcFilePath.toLatin1().constData(), NULL, NULL ) < 0 )
        return false;
    if( !formatCtx )
        return false;

    QDir srcFileDir = QFileInfo(srcFilePath).dir();
    const QString& tempFileName = lit("~%1%2.tmp.%3").arg(QDateTime::currentMSecsSinceEpoch())
            .arg(nx::utils::random::number()).arg(QLatin1String(formatCtx->iformat->name));
    const QString& tempFilePath = lit("%1/%2").arg(srcFileDir.path()).arg(tempFileName);

    //setting audio/video codecID
    for( size_t i = 0; i < formatCtx->nb_streams; ++i )
    {
        if( formatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO )
            formatCtx->audio_codec_id = formatCtx->streams[i]->codec->codec_id;
        else if( formatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO )
            formatCtx->video_codec_id = formatCtx->streams[i]->codec->codec_id;
    }

    std::unique_ptr<FileTranscoder> transcoder(new FileTranscoder());
    if( !transcoder->setSourceFile( srcFilePath ) ||
        !transcoder->setDestFile( tempFilePath ) ||
        !transcoder->setContainer( QLatin1String(formatCtx->iformat->name) ) ||
        !transcoder->setAudioCodec( formatCtx->audio_codec_id, QnTranscoder::TM_DirectStreamCopy ) ||
        !transcoder->setVideoCodec( formatCtx->video_codec_id, QnTranscoder::TM_DirectStreamCopy ) ||
        !transcoder->addTag( name, value ) )
    {
        avformat_close_input(&formatCtx);
        return false;
    }

    if( !transcoder->doSyncTranscode() )
    {
        avformat_close_input(&formatCtx);
        return false;
    }
    transcoder.reset();

    avformat_close_input(&formatCtx);

    //replacing source file with new file
    if( !srcFileDir.remove(QnFile::fileName(srcFilePath)) )
        return false;
    return srcFileDir.rename( tempFileName, QnFile::fileName(srcFilePath) );
}

void FileTranscoder::pleaseStop()
{
    QnLongRunnable::pleaseStop();

    QnMutexLocker lk( &m_mutex );
    m_cond.wakeAll();
}

bool FileTranscoder::startAsync()
{
    QnMutexLocker lk( &m_mutex );

    if( !openFiles() )
        return false;

    if( !isRunning() )
    {
        start();
        if( !isRunning() )
            return false;
    }

    m_resultCode = 0;
    m_state = sWorking;
    m_transcodedDataDuration = 0;

    m_cond.wakeAll();
    return true;
}

bool FileTranscoder::doSyncTranscode()
{
    if( !startAsync() )
        return false;

    QnMutexLocker lk( &m_mutex );
    while( m_state == sWorking )
        m_cond.wait( lk.mutex() );

    return m_resultCode == 0;
}

static const qint64 USEC_IN_MSEC = 1000;

void FileTranscoder::run()
{
    QnByteArray outPacket( 1, 0 );

    qint64 prevSrcPacketTimestamp = -1;
    qint64 srcUSecRead = 0;

    QnMutexLocker lk( &m_mutex );
    while( !needToStop() )
    {
        while( m_state < sWorking && !needToStop() )
            m_cond.wait( lk.mutex() );

        lk.unlock();
        if( needToStop() )
            break;
        lk.relock();

        //reading source
        QnAbstractMediaDataPtr dataPacket = m_mediaFileReader->getNextData();
        if( !dataPacket )
        {
            //end of file reached
            m_dest->waitForBytesWritten( -1 );
            m_state = sReady;
            prevSrcPacketTimestamp = -1;
            srcUSecRead = 0;
            m_cond.wakeAll();
            closeFiles();
            emit done(m_dstFilePath);
            continue;
        }

        //calculating read source data length
        if( prevSrcPacketTimestamp == -1 )
            prevSrcPacketTimestamp = dataPacket->timestamp;
        srcUSecRead += dataPacket->timestamp - prevSrcPacketTimestamp;
        prevSrcPacketTimestamp = dataPacket->timestamp;

        //transcoding
        m_resultCode = m_transcoder.transcodePacket( dataPacket, &outPacket );
        if( m_resultCode ||
            (m_transcodeDurationLimit > 0 && (srcUSecRead / USEC_IN_MSEC) >= m_transcodeDurationLimit) )   //checking transcode data length limit
        {
            m_dest->waitForBytesWritten( -1 );
            m_state = sReady;
            prevSrcPacketTimestamp = -1;
            srcUSecRead = 0;
            m_cond.wakeAll();
            closeFiles();
            emit done(m_dstFilePath);
            continue;
        }

        //writing to destination
        for( unsigned int curPos = 0; curPos < outPacket.size(); )
        {
            const qint64 bytesWritten = m_dest->write( outPacket.constData() + curPos, outPacket.size() - curPos );
            if( bytesWritten < 0 )
            {
                //write error occured. Interrupting transcoding
                m_resultCode = bytesWritten;
                m_state = sReady;
                prevSrcPacketTimestamp = -1;
                srcUSecRead = 0;
                m_cond.wakeAll();
                closeFiles();
                emit done(m_dstFilePath);
                break;
            }
            curPos += bytesWritten;
            if( curPos < outPacket.size() )
            {
                //could not read all data
                if( !m_dest->waitForBytesWritten( -1 ) )
                {
                    m_resultCode = bytesWritten;
                    m_state = sReady;
                    prevSrcPacketTimestamp = -1;
                    srcUSecRead = 0;
                    m_cond.wakeAll();
                    closeFiles();
                    emit done(m_dstFilePath);
                    break;
                }
            }
        }
    }
}

void FileTranscoder::setSource( QIODevice* /*src*/ )
{
    //TODO/IMPL need support in QnAviArchiveDelegate
}

void FileTranscoder::setDest( QIODevice* dest )
{
    m_dest.reset( dest );
}

bool FileTranscoder::openFiles()
{
    std::unique_ptr<QnAviArchiveDelegate> mediaFileReader( new QnAviArchiveDelegate() );

    QnResourcePtr res( new DummyResource() );
    res->setUrl( m_srcFilePath );
    if( !mediaFileReader->open( res , nullptr /*archiveInterityWatcher*/) )
        return false;

    if( !m_dest->open( QIODevice::WriteOnly ) )
        return false;

    m_mediaFileReader = std::move(mediaFileReader);
    m_mediaFileReader->setAudioChannel( 0 );
    return true;
}

void FileTranscoder::closeFiles()
{
    m_dest->close();
    m_mediaFileReader.reset( NULL );
}

#endif // ENABLE_DATA_PROVIDERS
