/**********************************************************
* 29 apr 2013
* a.kolesnikov
***********************************************************/

#include "audio_player.h"

#include <memory>

#include <QMutexLocker>

#include <core/resource/resource.h>
#include <plugins/resources/archive/avi_files/avi_archive_delegate.h>
#include <utils/common/util.h>

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
    m_state( sInit )
{
    if( !filePath.isEmpty() )
        open( filePath );
}

AudioPlayer::~AudioPlayer()
{
    pleaseStop();
    {
        QMutexLocker lk( &m_mutex );
        m_adaptiveSleep.breakSleep();
        m_cond.wakeAll();
    }
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

int AudioPlayer::playbackResultCode() const
{
    //TODO/IMPL
    return 0;
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

QString AudioPlayer::getTagValue( const QString& filePath, const QString& tagName )
{
    std::auto_ptr<AudioPlayer> audioPlayer( new AudioPlayer() );
    if( !audioPlayer->open( filePath ) )
        return QString();
    return audioPlayer->getTagValue( tagName );
}

bool AudioPlayer::open( const QString& filePath )
{
    QMutexLocker lk( &m_mutex );

    if( isOpenedNonSafe() )
        closeNonSafe();

    std::auto_ptr<QnAviArchiveDelegate> mediaFileReader( new QnAviArchiveDelegate() );

    QnResourcePtr res( new LocalAudioFileResource() );
    res->setUrl( filePath );
    res->setStatus( QnResource::Online );
    if( !mediaFileReader->open( res ) )
        return false;

    m_mediaFileReader = mediaFileReader.release();
    m_mediaFileReader->setAudioChannel( 0 );

    static const int AUDIO_BUF_SIZE = 4000;
    m_renderer = new QnAudioStreamDisplay( AUDIO_BUF_SIZE, AUDIO_BUF_SIZE / 2 );

    m_filePath = filePath;
    m_state = sReady;

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
    m_state = sPlaying;

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
        while( (m_state < sPlaying || !m_mediaFileReader) && !needToStop() )
            m_cond.wait( lk.mutex() );

        lk.unlock();
        if( needToStop() )
            break;
        lk.relock();

            //reading frame
        QnAbstractMediaDataPtr dataPacket = m_mediaFileReader->getNextData();
        if( !dataPacket )
        {
            m_renderer->flush();

            //end of file reached
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
}

bool AudioPlayer::isOpenedNonSafe() const
{
    return m_mediaFileReader != NULL;
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
