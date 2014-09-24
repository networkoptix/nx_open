/**********************************************************
* 29 apr 2013
* a.kolesnikov
***********************************************************/

#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <memory>

#include <QtCore/QBuffer>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>

#include <core/datapacket/abstract_data_packet.h>
#include <plugins/storage/memory/ext_iodevice_storage.h>
#include <utils/common/adaptive_sleep.h>
#include <utils/common/long_runnable.h>


class QnAviArchiveDelegate;
class QnAudioStreamDisplay;

//!Plays audio file
/*!
    Underlying API uses ffmpeg, so any ffmpeg-supported file format can be used. If file contains multiple audio streams, it is unspecified which one is played
*/
class AudioPlayer
:
    public QnLongRunnable
{
    Q_OBJECT

public:
    // TODO: #Elric #enum
    enum ResultCode
    {
        rcNoError,
        rcSynthesizingError,
        rcMediaInitializationError
    };

    /*!
        \param filePath If not empty, calls \a open(\a filePath)
    */
    AudioPlayer( const QString& filePath = QString() );
    virtual ~AudioPlayer();

    //!Returns true, if media file opened successfully
    bool isOpened() const;
    //!Returns tag \a name from file. Can be used on opened file
    QString getTagValue( const QString& name ) const;
    //!Returns zero on success else - error code (TODO: define error codes)
    ResultCode playbackResultCode() const;

    /*!
        \note \a dataSource removed in any case
    */
    static bool playAsync( QIODevice* dataSource );
    //!Starts playing file, returns immediately
    /*!
        Creates \a AudioPlayer object, which is removed on playback finish
        \param target       Signal will be sent to this target on play end (can be null).
        \param slot         Slot that will be called on play end if target is not null.
        \return             True if playback started, false - otherwise
    */
    static bool playFileAsync( const QString& filePath, QObject* target = NULL, const char *slot = NULL );

    //!Generates wav from \a text and plays it...
    /*!
        Creates \a AudioPlayer object, which is removed on playback finish
        \param target       Signal will be sent to this target on play end (can be null).
        \param slot         Slot that will be called on play end if target is not null.
        \return             True if playback started, false - otherwise
    */
    static bool sayTextAsync( const QString& text, QObject* target = NULL, const char *slot = NULL );
    //!Reads tag \a tagName
    /*!
        This method is synchronous
    */
    static QString getTagValue( const QString& filePath, const QString& tagName );

public slots:
    //!Overrides QnLongRunnable::pleaseStop
    virtual void pleaseStop() override;
    //!Openes data source for playback and initializes internal data
    /*!
        \param dataSource MUST be opened for reading
        \note \a dataSource is removed in any case (event if this method fails)
        \note If there is opened file, it's being closed first
        \note Ownership of \a dataSource is passed to this object. \a dataSource is destroyed after playback done
    */
    bool open( QIODevice* dataSource );
    //!Openes file for playback and initializes internal data
    /*!
        If there is opened file, it's being closed first
    */
    bool open( const QString& filePath );
    //!Prepares to play \a text
    /*!
        Subsequent \a AudioPlayer::playAsync call will synthesize speech and play it.
        \note Speech synthesizing takes place in internal worker thread
    */
    bool prepareTextPlayback( const QString& text );
    //!Starts playing file
    /*!
        This method returns immediately and playback is performed asynchronously.
        Uses file path, provided to constructor or to \a open call.
        \return false, if no opened file. true, if started playback
        \note If file is being already played, nothing is done and \a true is returned
    */
    bool playAsync();
    //!Closes opened file. If file not opened, does nothing
    void close();

protected:
    virtual void run();

signals:
    //!Emitted on playback finish (successfully or failed). For result see \a resultCode method
    void done();

private:
    // TODO: #Elric #enum
    enum State
    {
        sInit,
        sReady,                 //!< file opened successfully
        sSynthesizing,          //!< synthesizing speech from text
        sSynthesizingAutoPlay,  //!< synthesizing speech from text. Playback will start just after synthesizing is complete
        sPlaying                //!< file being played. On reaching end of file, state moves to \ ready
    };

    QString m_filePath;
    QnAviArchiveDelegate* m_mediaFileReader;
    QnAdaptiveSleep m_adaptiveSleep;
    QnAudioStreamDisplay* m_renderer;
    qint64 m_rtStartTime;
    qint64 m_lastRtTime;
    mutable QMutex m_mutex;
    QWaitCondition m_cond;
    State m_state;
    QnExtIODeviceStorageResourcePtr m_storage;
    std::auto_ptr<QBuffer> m_synthesizingTarget;
    QString m_textToPlay;
    ResultCode m_resultCode;

    bool openNonSafe( QIODevice* dataSource );
    bool isOpenedNonSafe() const;
    void closeNonSafe();
    void doRealtimeDelay( const QnAbstractDataPacketPtr& media );
};

#endif  //AUDIO_PLAYER_H
