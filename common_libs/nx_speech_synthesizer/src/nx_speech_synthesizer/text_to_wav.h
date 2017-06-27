#if !defined(EDGE_SERVER) && !defined(DISABLE_FESTIVAL)

#ifndef TEXT_TO_WAV_H
#define TEXT_TO_WAV_H

#include <QtCore/QIODevice>
#include <QtCore/QSharedPointer>
#include <QtCore/QString>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/singleton.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/long_runnable.h>
#include <utils/common/threadqueue.h>
#include <utils/media/audioformat.h>

// TODO: #Elric this header does not belong in the source root.

//!Synthesizes wav based on \a text. Uses festival engine
/*!
    \param text Only latin1 string is supported
    \note Has internal thread. Holds queue of text to synthetise
*/
class TextToWaveServer:
    public QnLongRunnable,
    public Singleton<TextToWaveServer>
{
    Q_OBJECT

public:
    TextToWaveServer(const QString& binaryPath);
    virtual ~TextToWaveServer();

    void waitForStarted();

    virtual void pleaseStop() override;

public slots:
    //!Adds task to the queue
    /*!
        \return ID of generation context. 0 in case of error
    */
    int generateSoundAsync( const QString& text, QIODevice* const dest );
    //!Adds task to the queue and blocks till speech is generated (or failed)
    /*!
        \return generation result
        \note This method is synchronous and reentrant
    */
    bool generateSoundSync(
        const QString& text,
        QIODevice* const dest,
        QnAudioFormat* outFormat = nullptr);

signals:
    //!Emitted in any case on text generation done (successful or not)
    void done( int textID, bool result );

protected:
    virtual void run() override;

private:
    class SynthetiseSpeechTask
    {
    public:
        int id;
        QString text;
        QIODevice* dest;
        QnAudioFormat format;
        bool result;
        bool done;

        SynthetiseSpeechTask()
        :
            id( 0 ),
            dest(nullptr),
            format(QnAudioFormat()),
            result( false ),
            done( false )
        {
        }
    };

    QString m_binaryPath;
    QnSafeQueue<QSharedPointer<SynthetiseSpeechTask> > m_textQueue;
    QAtomicInt m_prevTaskID;
    QnWaitCondition m_cond;
    QnMutex m_mutex;

    nx::utils::promise<void> m_initializedPromise;
    nx::utils::future<void> m_initializedFuture;

    QSharedPointer<SynthetiseSpeechTask> addTaskToQueue( const QString& text, QIODevice* const dest );
};

#endif  //TEXT_TO_WAV_H

#endif
