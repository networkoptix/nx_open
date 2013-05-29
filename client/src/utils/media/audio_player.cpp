/**********************************************************
* 29 apr 2013
* a.kolesnikov
***********************************************************/

#include "audio_player.h"

#include <memory>

#include <QBuffer>
#include <QMutexLocker>

#include <core/resource/resource.h>
#include <plugins/resources/archive/avi_files/avi_archive_delegate.h>
#include <utils/common/util.h>
#include <text_to_wav.h>

#include "../../camera/audio_stream_display.h"


class LocalAudioFileResource
:
    public QnResource
{
public:
    virtual QString getUniqueId() const { return QString(); }
};


AudioPlayer::AudioPlayer( const QString& filePath )
:
    m_mediaFileReader( NULL ),
    m_adaptiveSleep( MAX_FRAME_DURATION*1000 ),
    m_renderer( NULL ),
    m_rtStartTime( AV_NOPTS_VALUE ),
    m_lastRtTime( 0 ),
    m_state( sInit ),
    m_storage( new QnExtIODeviceStorageResource() ),
    m_synthesizingTarget( NULL ),
    m_resultCode( rcNoError )
{
    if( !filePath.isEmpty() )
        open( filePath );
}

AudioPlayer::~AudioPlayer()
{
    pleaseStop();
    stop();

    close();
}

bool AudioPlayer::isOpened() const
{
    QMutexLocker lk( &m_mutex );
    return isOpenedNonSafe();
}

QString AudioPlayer::getTagValue( const QString& name ) const
{
    const char* tagValueCStr = m_mediaFileReader->getTagValue( name.toUtf8().data() );
    return tagValueCStr ? QString::fromUtf8(tagValueCStr) : QString();
}

AudioPlayer::ResultCode AudioPlayer::playbackResultCode() const
{
    return m_resultCode;
}

bool AudioPlayer::playAsync( QIODevice* dataSource )
{
    std::auto_ptr<AudioPlayer> audioPlayer( new AudioPlayer() );
    if( !audioPlayer->open( dataSource ) )
        return false;

    connect( audioPlayer.get(), SIGNAL(done()), audioPlayer.get(), SLOT(deleteLater()) );
    if( !audioPlayer->playAsync() )
        return false;
    audioPlayer.release();
    return true;
}

bool AudioPlayer::playFileAsync( const QString& filePath )
{
    std::auto_ptr<AudioPlayer> audioPlayer( new AudioPlayer() );
    if( !audioPlayer->open( filePath ) )
        return false;

    connect( audioPlayer.get(), SIGNAL(done()), audioPlayer.get(), SLOT(deleteLater()) );
    if( !audioPlayer->playAsync() )
        return false;
    audioPlayer.release();
    return true;
}

bool AudioPlayer::sayTextAsync( const QString& text )
{
    std::auto_ptr<AudioPlayer> audioPlayer( new AudioPlayer() );
    if( !audioPlayer->prepareTextPlayback( text ) )
        return false;

    connect( audioPlayer.get(), SIGNAL(done()), audioPlayer.get(), SLOT(deleteLater()) );
    if( !audioPlayer->playAsync() )
        return false;
    audioPlayer.release();
    return true;
}

QString AudioPlayer::getTagValue( const QString& filePath, const QString& tagName )
{
    std::auto_ptr<AudioPlayer> audioPlayer( new AudioPlayer() );
    if( !audioPlayer->open( filePath ) )
        return QString();
    return audioPlayer->getTagValue( tagName );
}

static const int AUDIO_BUF_SIZE = 4000;

void AudioPlayer::pleaseStop()
{
    QnLongRunnable::pleaseStop();

    QMutexLocker lk( &m_mutex );
    m_adaptiveSleep.breakSleep();
    m_cond.wakeAll();
}

bool AudioPlayer::open( QIODevice* dataSource )
{
    QMutexLocker lk( &m_mutex );

    m_resultCode = rcNoError;
    if( isOpenedNonSafe() )
        closeNonSafe();
    if( !openNonSafe( dataSource ) )
        return false;
    m_state = sReady;
    return true;
}

bool AudioPlayer::open( const QString& filePath )
{
    std::auto_ptr<QFile> srcFile( new QFile( filePath ) );
    if( !srcFile->open(QFile::ReadOnly) )
        return false;
    return open( srcFile.release() );
}

bool AudioPlayer::prepareTextPlayback( const QString& text )
{
    QMutexLocker lk( &m_mutex );

    m_resultCode = rcNoError;

    m_synthesizingTarget.reset( new QBuffer() );

    //NOTE cannot open at the moment, since no media data yet

    //starting speech synthesizing
    m_textToPlay = text;
    m_cond.wakeAll();
    m_state = sSynthesizing;

    return true;
}

bool AudioPlayer::playAsync()
{
    QMutexLocker lk( &m_mutex );

    if( !isOpenedNonSafe() )
        return false;

    if( !isRunning() )
    {
        start();
        if( !isRunning() )
            return false;
    }
    m_cond.wakeAll();
    m_state = sSynthesizing ? sSynthesizingAutoPlay : sPlaying;

    return true;
}

void AudioPlayer::close()
{
    QMutexLocker lk( &m_mutex );
    closeNonSafe();
}

void AudioPlayer::run()
{
    QMutexLocker lk( &m_mutex );
    for( ;; )
    {
        while( m_state <= sReady && !needToStop() )
            m_cond.wait( lk.mutex() );

        lk.unlock();
        if( needToStop() )
            break;
        lk.relock();

        switch( m_state )
        {
            case sSynthesizing:
            case sSynthesizingAutoPlay:
            {
                if( !m_synthesizingTarget->open( QIODevice::WriteOnly ) ||
                    !textToWav( m_textToPlay, m_synthesizingTarget.get() ) )
                {
                    emit done();
                    m_state = sReady;
                    m_resultCode = rcSynthesizingError;
                    continue;
                }
                m_synthesizingTarget->close();
                if( m_state == sSynthesizingAutoPlay )
                    m_state = sPlaying;
                else
                    m_state = sReady;

                //opening media
                if( !m_synthesizingTarget->open( QIODevice::ReadOnly ) ||
                    !openNonSafe( m_synthesizingTarget.release() ) )
                {
                    emit done();
                    m_state = sReady;
                    m_resultCode = rcMediaInitializationError;
                    continue;
                }
                break;
            }

            case sPlaying:
            {
                    //reading frame
                QnAbstractMediaDataPtr dataPacket = m_mediaFileReader->getNextData();
                if( !dataPacket )
                {
                    //end of file reached
                    m_renderer->playCurrentBuffer();

                    //TODO: #ak IMPL block till playback is finished
                    msleep( AUDIO_BUF_SIZE / 2 );

                    emit done();
                    m_state = sReady;
                    continue;
                }

                QnCompressedAudioDataPtr audioData = dataPacket.dynamicCast<QnCompressedAudioData>();
                if( !audioData )
                    continue;

                    //delay
                doRealtimeDelay( dataPacket );

                    //playing it
                m_renderer->putData( audioData );
                break;
            }
        }
    }
}

void AudioPlayer::closeNonSafe()
{
    if( m_mediaFileReader )
    {
        delete m_mediaFileReader;
        m_mediaFileReader = NULL;
    }

    if( m_renderer )
    {
        delete m_renderer;
        m_renderer = NULL;
    }

    m_filePath.clear();
    m_state = sInit;
    m_synthesizingTarget.reset();
}

bool AudioPlayer::openNonSafe( QIODevice* dataSource )
{
    const QString& temporaryFilePath = QString::number(rand());
    const QString& temporaryResUrl = QString::fromAscii("%1://%2").arg(QLatin1String("qiodev")).arg(temporaryFilePath);
    m_storage->registerResourceData( temporaryFilePath, dataSource );

    std::auto_ptr<QnAviArchiveDelegate> mediaFileReader( new QnAviArchiveDelegate() );
    mediaFileReader->setStorage( m_storage );

    QnResourcePtr res( new LocalAudioFileResource() );
    res->setUrl( temporaryResUrl );
    res->setStatus( QnResource::Online );
    if( !mediaFileReader->open( res ) )
    {
        m_storage->removeFile( temporaryFilePath );
        return false;
    }

    m_mediaFileReader = mediaFileReader.release();
    m_mediaFileReader->setAudioChannel( 0 );

    m_renderer = new QnAudioStreamDisplay( AUDIO_BUF_SIZE, AUDIO_BUF_SIZE / 2 );

    m_filePath = temporaryResUrl;

    return true;
}

bool AudioPlayer::isOpenedNonSafe() const
{
    return m_state >= sReady || m_mediaFileReader != NULL;
}

void AudioPlayer::doRealtimeDelay( const QnAbstractDataPacketPtr& media )
{
    if( m_rtStartTime == (qint64)AV_NOPTS_VALUE )
    {
        m_rtStartTime = media->timestamp;
    }
    else
    {
        qint64 timeDiff = media->timestamp - m_lastRtTime;
        if( timeDiff <= MAX_FRAME_DURATION*1000 )
            m_adaptiveSleep.terminatedSleep(timeDiff, MAX_FRAME_DURATION*1000); // if diff too large, it is recording hole. do not calc delay for this case
    }
    m_lastRtTime = media->timestamp;
}
