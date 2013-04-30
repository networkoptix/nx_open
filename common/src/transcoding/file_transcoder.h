/**********************************************************
* 29 apr 2013
* a.kolesnikov
***********************************************************/

#ifndef FILE_TRANSCODER_H
#define FILE_TRANSCODER_H

#include <memory>

#include <QIODevice>
#include <QMutex>
#include <QWaitCondition>

#include <libavcodec/avcodec.h>

#include <core/resource/media_resource.h>
#include <plugins/resources/archive/avi_files/avi_archive_delegate.h>
#include <transcoding/ffmpeg_transcoder.h>
#include <utils/common/long_runnable.h>


class QnAviArchiveDelegate;

//!Helper class to transcoder file to other file
/*!
    Transcoding is done in a separate thread.
    Modifying transcoding parameters after transcoding start can cause undefined behavour
    \note Object belongs to the thread, it has been created in (not internal thread)
    \note Class methods are not thread-safe
*/
class FileTranscoder
:
    public QnLongRunnable
{
    Q_OBJECT

public:
    FileTranscoder();
    virtual ~FileTranscoder();

    /*!
        \return false on file access error
    */
    bool setSourceFile( const QString& filePath );
    /*!
        \return false on file access error
    */
    bool setDestFile( const QString& filePath );

    //!Set ffmpeg container to mux to
    /*!
        \return Returns \a true on success
    */
    bool setContainer( const QString& containerName );
    //!Add tag to output file
    /*!
        Tagging MUST be done after \a setContainer and before \a startAsync
    */
    bool addTag( const QString& name, const QString& value );
    //!Set ffmpeg video codec and params
    /*!
        \param codec codec to transcode to. Ignored if \a transcodeMethod == \a TM_NoTranscode
        \param transcodeMethod Underlying transcoder to use
        \param resolution output resolution. If \a QSize(0,0), then origin resolution is used. Not used if transcode method \a TM_NoTranscode
        \param bitrate Bitrate after transcode. By default bitrate is autodetected. Not used if transcode method \a TM_NoTranscode
        \param params codec params. Not used if transcode method \a TM_NoTranscode
        \return Returns \a true on success
    */
    bool setVideoCodec(
        CodecID codec,
        QnTranscoder::TranscodeMethod transcodeMethod = QnTranscoder::TM_FfmpegTranscode,
        QnStreamQuality quality = QnQualityHighest,
        const QSize& resolution = QSize(0,0),
        int bitrate = -1,
        QnCodecParams::Value params = QnCodecParams::Value() );
    //!Set ffmpeg audio codec
    /*!
        \param codec codec to transcode to. Ignored if \a transcodeMethod == \a TM_NoTranscode
        \param transcodeMethod Underlying transcoder to use
        \return Returns \a true on success
    */
    bool setAudioCodec(
        CodecID codec,
        QnTranscoder::TranscodeMethod transcodeMethod = QnTranscoder::TM_FfmpegTranscode );
    //!Set source data duration (in millis) to read. By default, whole source data is read
    void setTranscodeDurationLimit( unsigned int lengthToReadMS );
    //!Returns zero on success else - error code (TODO: define error codes)
    int resultCode() const;

public slots:
    //!Start transcoding
    /*!
        This method returns immediately. On transcoding end \a done signal is emmitted
    */
    bool startAsync();
    //!Do transcoding in sync mode
    bool doSyncTranscode();

signals:
    //!Emitted on transcoding finish (successfully or failed). For result see \a resultCode method
    void done();

protected:
    virtual void run() override;

private:
    enum State
    {
        sInit,
        sReady,
        sWorking
    };

    mutable QMutex m_mutex;
    QWaitCondition m_cond;
    std::auto_ptr<QnAviArchiveDelegate> m_mediaFileReader;
    QnFfmpegTranscoder m_transcoder;
    int m_resultCode;
    State m_state;
    std::auto_ptr<QIODevice> m_dest;
    //!in millis. 0 - no limit
    unsigned int m_transcodeDurationLimit;
    unsigned int m_transcodedDataDuration;
    QString m_srcFilePath;

    /*!
        Takes ownership of \a src
    */
    void setSource( QIODevice* src );
    /*!
        Takes ownership of \a dest
    */
    void setDest( QIODevice* dest );
    bool openFiles();
    void closeFiles();
};

#endif  //FILE_TRANSCODER_H
