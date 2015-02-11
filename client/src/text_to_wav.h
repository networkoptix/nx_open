#ifndef TEXT_TO_WAV_H
#define TEXT_TO_WAV_H

#include <QtCore/QIODevice>
#include <utils/common/mutex.h>
#include <QtCore/QSharedPointer>
#include <QtCore/QString>
#include <utils/common/wait_condition.h>

#include <utils/common/long_runnable.h>
#include <utils/common/threadqueue.h>
#include <utils/common/singleton.h>

// TODO: #Elric this header does not belong in the source root.

//!Synthesizes wav based on \a text. Uses festival engine
/*!
    \param text Only latin1 string is supported
    \note Has internal thread. Holds queue of text to synthetise
*/
class TextToWaveServer
:
    public QnLongRunnable, 
    public Singleton<TextToWaveServer>
{
    Q_OBJECT

public:
    TextToWaveServer();
    virtual ~TextToWaveServer();

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
        \note This method is synchronous and reenterable
    */
    bool generateSoundSync( const QString& text, QIODevice* const dest );

signals:
    //!Emmitted in any case on text generation done (successfull or not)
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
        bool result;
        bool done;

        SynthetiseSpeechTask()
        :
            id( 0 ),
            dest( NULL ),
            result( false ),
            done( false )
        {
        }
    };

    CLThreadQueue<QSharedPointer<SynthetiseSpeechTask> > m_textQueue;
    QAtomicInt m_prevTaskID;
    QnWaitCondition m_cond;
    QnMutex m_mutex;

    QSharedPointer<SynthetiseSpeechTask> addTaskToQueue( const QString& text, QIODevice* const dest );
};

#endif  //TEXT_TO_WAV_H
