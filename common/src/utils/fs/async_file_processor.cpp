/**********************************************************
* 26 sep 2013
* a.kolesnikov
***********************************************************/

#include "async_file_processor.h"

#include <sys/stat.h>

#include <memory>
#include <typeinfo>


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

class AsyncStatTask
:
    public FileTask
{
public:
    QString filePath;
    std::unique_ptr<AsyncFileProcessor::AbstractAsyncStatHandler> handler;

    AsyncStatTask(
        const QString& _filePath,
        std::unique_ptr<AsyncFileProcessor::AbstractAsyncStatHandler> _handler )
    :
        FileTask( nullptr ),
        filePath( _filePath ),
        handler( std::move(_handler) )
    {
    }
};


AsyncFileProcessor::AsyncFileProcessor()
{
    setObjectName( QLatin1String( "AsyncFileProcessor" ) );
    start();
}

AsyncFileProcessor::~AsyncFileProcessor()
{
    pleaseStop();
    wait();
}

void AsyncFileProcessor::pleaseStop()
{
    m_taskQueue.push( nullptr );
}

bool AsyncFileProcessor::fileWrite(
    const std::shared_ptr<QnFile>& file,
    const QByteArray& buffer,
    QnFile::AbstractWriteHandler* handler )
{
    return m_taskQueue.push( new WriteFileTask( file, buffer, handler ) );
}

bool AsyncFileProcessor::fileClose(
    const std::shared_ptr<QnFile>& file,
    QnFile::AbstractCloseHandler* handler )
{
    return m_taskQueue.push( new CloseFileTask( file, handler ) );
}

AsyncFileProcessor* AsyncFileProcessor::instance()
{
    static AsyncFileProcessor _instance;
    return &_instance;
}

void AsyncFileProcessor::run()
{
    while( !needToStop() )
    {
        FileTask* taskPtr = nullptr;
        m_taskQueue.pop( taskPtr );
        if( !taskPtr )
            break;

        std::unique_ptr<FileTask> task( taskPtr );

        if( typeid(*task.get()) == typeid(OpenFileTask) )
            doOpenFile( static_cast<const OpenFileTask*>(task.get()) );
        else if( typeid(*task.get()) == typeid(WriteFileTask) )
            doWriteFile( static_cast<const WriteFileTask*>(task.get()) );
        else if( typeid(*task.get()) == typeid(CloseFileTask) )
            doCloseFile( static_cast<const CloseFileTask*>(task.get()) );
        else if( typeid(*task.get()) == typeid(AsyncStatTask) )
            doStat( static_cast<const AsyncStatTask*>(task.get()) );
    }
}

void AsyncFileProcessor::doOpenFile( const OpenFileTask* /*task*/ )
{
    //TODO #ak implement and use in applauncher
}

void AsyncFileProcessor::doWriteFile( const WriteFileTask* task )
{
    const qint64 bytesWritten = task->file->write( task->buffer.constData(), task->buffer.size() );
    task->handler->onAsyncWriteFinished(
        task->file,
        bytesWritten,
        bytesWritten == -1 ? SystemError::getLastOSErrorCode() : SystemError::noError );
}

void AsyncFileProcessor::doCloseFile( const CloseFileTask* task )
{
    task->file->close();
    task->handler->onAsyncCloseFinished( task->file, SystemError::noError );
}

void AsyncFileProcessor::doStat( const AsyncStatTask* task )
{
#ifdef _WIN32
    struct _stat64 fInfo;
    if( _wstat64( reinterpret_cast<const wchar_t*>(task->filePath.utf16()), &fInfo ) < 0 )
#else
    struct stat64 fInfo;
    if( stat64( task->filePath.toUtf8().constData(), &fInfo ) < 0 )
#endif
        task->handler->done( SystemError::getLastOSErrorCode(), -1 );
    else
        task->handler->done( SystemError::noError, fInfo.st_size );
}

bool AsyncFileProcessor::statAsyncImpl(
    const QString& filePath,
    std::unique_ptr<AsyncFileProcessor::AbstractAsyncStatHandler> handler )
{
    return m_taskQueue.push( new AsyncStatTask( filePath, std::move(handler) ) );
}
