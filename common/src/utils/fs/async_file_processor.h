/**********************************************************
* 26 sep 2013
* a.kolesnikov
***********************************************************/

#ifndef ASYNC_FILE_PROCESSOR_H
#define ASYNC_FILE_PROCESSOR_H

#include "file.h"

#include "../common/long_runnable.h"
#include "../common/threadqueue.h"


class AsyncFileProcessor
:
    public QnLongRunnable
{
public:
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

    static AsyncFileProcessor* instance();

protected:
    //!Implementation of QThread::run
    virtual void run() override;

private:
    class FileTask
    {
    public:
        std::shared_ptr<QnFile> file;

        FileTask( const std::shared_ptr<QnFile>& _file )
        :
            file( _file )
        {
        }

        virtual ~FileTask() {}
    };

    class OpenFileTask
    :
        public FileTask
    {
    public:
        OpenFileTask( const std::shared_ptr<QnFile>& _file )
        :
            FileTask( _file )
        {
        }
    };

    class WriteFileTask
    :
        public FileTask
    {
    public:
        const QByteArray buffer;
        QnFile::AbstractWriteHandler* const handler;

        WriteFileTask(
            const std::shared_ptr<QnFile>& _file,
            const QByteArray& _buffer,
            QnFile::AbstractWriteHandler* const _handler )
        :
            FileTask( _file ),
            buffer( _buffer ),
            handler( _handler )
        {
        }
    };

    class CloseFileTask
    :
        public FileTask
    {
    public:
        QnFile::AbstractCloseHandler* const handler;

        CloseFileTask(
            const std::shared_ptr<QnFile>& _file,
            QnFile::AbstractCloseHandler* const _handler )
        :
            FileTask( _file ),
            handler( _handler )
        {
        }
    };

    CLThreadQueue<FileTask*> m_taskQueue;

    void doOpenFile( const OpenFileTask* task );
    void doWriteFile( const WriteFileTask* task );
    void doCloseFile( const CloseFileTask* task );
};

#endif  //ASYNC_FILE_PROCESSOR_H
