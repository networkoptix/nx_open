/**********************************************************
* 29 apr 2013
* a.kolesnikov
***********************************************************/

#include "file_transcoder.h"

#include <QMutexLocker>

#include <core/resource/resource.h>


class DummyResource
:
    public QnResource
{
public:
    virtual QString getUniqueId() const { return QString(); }
};

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
    {
        QMutexLocker lk( &m_mutex );
        m_cond.wakeAll();
    }
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
    return m_transcoder.setContainer( containerName ) == 0;
}

bool FileTranscoder::addTag( const QString& name, const QString& value )
{
    return m_transcoder.addTag( name, value );
}

bool FileTranscoder::setVideoCodec(
    CodecID codec,
    QnTranscoder::TranscodeMethod transcodeMethod,
    QnStreamQuality quality,
    const QSize& resolution,
    int bitrate,
    QnCodecParams::Value params )
{
    return m_transcoder.setVideoCodec( codec, transcodeMethod, quality, resolution, bitrate, params ) == 0;
}

bool FileTranscoder::setAudioCodec(
    CodecID codec,
    QnTranscoder::TranscodeMethod transcodeMethod )
{
    return m_transcoder.setAudioCodec( codec, transcodeMethod );
}

void FileTranscoder::setTranscodeDurationLimit( unsigned int lengthToReadMS )
{
    m_transcodeDurationLimit = lengthToReadMS;
}

int FileTranscoder::resultCode() const
{
    return m_resultCode;
}

bool FileTranscoder::startAsync()
{
    QMutexLocker lk( &m_mutex );

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

    QMutexLocker lk( &m_mutex );
    while( m_state == sWorking )
        m_cond.wait( lk.mutex() );

    return m_resultCode == 0;
}

void FileTranscoder::run()
{
    QnByteArray outPacket( 1, 0 );

    QMutexLocker lk( &m_mutex );
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
            m_cond.wakeAll();
            closeFiles();
            emit done(m_dstFilePath);
            continue;
        }

        if( m_transcodeDurationLimit > 0 )
        {
            //TODO/IMPL checking transcode data length limit
        }

        //transcoding
        m_resultCode = m_transcoder.transcodePacket( dataPacket, &outPacket );
        if( m_resultCode )
        {
            m_dest->waitForBytesWritten( -1 );
            m_state = sReady;
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
                    m_cond.wakeAll();
                    closeFiles();
                    emit done(m_dstFilePath);
                    break;
                }
            }
        }
    }
}

void FileTranscoder::setSource( QIODevice* src )
{
    //TODO/IMPL need support in QnAviArchiveDelegate
}

void FileTranscoder::setDest( QIODevice* dest )
{
    m_dest.reset( dest );
}

bool FileTranscoder::openFiles()
{
    std::auto_ptr<QnAviArchiveDelegate> mediaFileReader( new QnAviArchiveDelegate() );

    QnResourcePtr res( new DummyResource() );
    res->setUrl( m_srcFilePath );
    res->setStatus( QnResource::Online );
    if( !mediaFileReader->open( res ) )
        return false;

    if( !m_dest->open( QIODevice::WriteOnly ) )
        return false;

    m_mediaFileReader = mediaFileReader;
    m_mediaFileReader->setAudioChannel( 0 );
    return true;
}

void FileTranscoder::closeFiles()
{
    m_dest->close();
    m_mediaFileReader.reset( NULL );
}
