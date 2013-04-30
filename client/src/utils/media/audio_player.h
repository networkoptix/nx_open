/**********************************************************
* 29 apr 2013
* a.kolesnikov
***********************************************************/

#ifndef AUDIO_PLAYER_H
#define AUDIO_PLAYER_H

#include <QMutex>
#include <QWaitCondition>

#include <core/datapacket/abstract_data_packet.h>
#include <utils/common/adaptive_sleep.h>
#include <utils/common/long_runnable.h>


class QnAviArchiveDelegate;
class QnAudioStreamDisplay;

//!Plays audio file
/*!
    Underlying API uses ffmpeg, so any ffmpeg-supported file format can be used. If file contains multiple audio streams, it is unspecified which one is played

    Object belongs to thread it was created in
*/
class AudioPlayer
:
    public QnLongRunnable
{
    Q_OBJECT

public:
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
    int playbackResultCode() const;

    //!Starts playing file, returns immediately
    /*!
        Creates \a AudioPlayer object, which is removed on playback finish
        \return true if playback started, false - otherwise
        \note With this method, there is no way to receive playback done event, or cancel playback
    */
    static bool playFileAsync( const QString& filePath );
    //!Reads tag \a tagName
    /*!
        This method is synchronous
    */
    static QString getTagValue( const QString& filePath, const QString& tagName );

public slots:
    //!Openes file for playback and initializes internal data
    /*!
        If there is opened file, it's being closed first
    */
    bool open( const QString& filePath );
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
    enum State
    {
        sInit,
        sReady,  //!< file opened successfully
        sPlaying //!< file being played. On reaching end of file, state moves to \ ready
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

    void doRealtimeDelay( const QnAbstractDataPacketPtr& media );
};

#endif  //AUDIO_PLAYER_H
