/**********************************************************
* 26 sep 2013
* a.kolesnikov
***********************************************************/

#ifndef ASYNC_FILE_PROCESSOR_H
#define ASYNC_FILE_PROCESSOR_H

#include "file.h"

#include <nx/utils/thread/long_runnable.h>

#include "../common/threadqueue.h"


class FileTask;
class OpenFileTask;
class WriteFileTask;
class CloseFileTask;
class AsyncStatTask;

class AsyncFileProcessor:
    public QnLongRunnable
{
public:
    //!This class for internal use only. MAY be removed or changed in future
    class AbstractAsyncStatHandler
    {
    public:
        virtual ~AbstractAsyncStatHandler() {}
        virtual void done( SystemError::ErrorCode errorCode, qint64 fileSize ) = 0;
    };

    AsyncFileProcessor();
    virtual ~AsyncFileProcessor();

    //!Implementation of \a QnLongRunnable::pleaseStop()
    virtual void pleaseStop() override;

    bool fileWrite(
        const std::shared_ptr<QnFile>& file,
        const QByteArray& buffer,
        QnFile::AbstractWriteHandler* handler );
    bool fileClose(
        const std::shared_ptr<QnFile>& file,
        QnFile::AbstractCloseHandler* handler );

    /*!
        \param handler Functor ( SystemError::ErrorCode errorCode, qint64 fileSize )
        \todo Not sure, if this is right place for this method. But, things will sort out in the process
    */
    template<class HandlerType>
        bool statAsync( const QString& filePath, HandlerType handler ) {
            return statAsyncImpl( filePath, std::unique_ptr<AbstractAsyncStatHandler>( new CustomAsyncStatHandler<HandlerType>(std::move(handler)) ) );
        }

    static AsyncFileProcessor* instance();

protected:
    //!Implementation of QThread::run
    virtual void run() override;

private:
    template<class HandlerFunc>
    class CustomAsyncStatHandler
    :
        public AbstractAsyncStatHandler
    {
    public:
        CustomAsyncStatHandler( const HandlerFunc& handler ) : m_handler( handler ) {}
        CustomAsyncStatHandler( const HandlerFunc&& handler ) : m_handler( handler ) {}

        virtual void done( SystemError::ErrorCode errorCode, qint64 fileSize ) override
        {
            m_handler( errorCode, fileSize );
        }

    private:
        HandlerFunc m_handler;
    };

    QnSafeQueue<FileTask*> m_taskQueue;

    void doOpenFile( const OpenFileTask* task );
    void doWriteFile( const WriteFileTask* task );
    void doCloseFile( const CloseFileTask* task );
    void doStat( const AsyncStatTask* task );

    bool statAsyncImpl( const QString& filePath, std::unique_ptr<AbstractAsyncStatHandler> handler );
};

#endif  //ASYNC_FILE_PROCESSOR_H
