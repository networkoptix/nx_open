#ifndef TEXT_TO_WAV_H
#define TEXT_TO_WAV_H

#include <QtCore/QIODevice>
#include <QtCore/QMutex>
#include <QtCore/QSharedPointer>
#include <QtCore/QString>
#include <QtCore/QWaitCondition>

#include <utils/common/long_runnable.h>
#include <utils/common/threadqueue.h>


//!Synthesizes wav based on \a text. Uses festival engine
/*!
    \param text Only latin1 string is supported
    \note Has internal thread. Holds queue of text to synthetise
*/
class TextToWaveServer
:
    public QnLongRunnable
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

    static void initStaticInstance( TextToWaveServer* );
    static TextToWaveServer* instance();

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
    QWaitCondition m_cond;
    QMutex m_mutex;

    QSharedPointer<SynthetiseSpeechTask> addTaskToQueue( const QString& text, QIODevice* const dest );
};

#endif  //TEXT_TO_WAV_H
