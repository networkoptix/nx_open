/**********************************************************
* 26 sep 2013
* a.kolesnikov
***********************************************************/

#include "async_file_processor.h"

#include <memory>
#include <typeinfo>


AsyncFileProcessor::AsyncFileProcessor()
{
    setObjectName( QLatin1String( "AsyncFileProcessor" ) );
    start();
}

AsyncFileProcessor::~AsyncFileProcessor()
{
    //TODO/IMPL
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
            continue;

        std::unique_ptr<FileTask> task( taskPtr );

        //const std::type_info& t1 = typeid(*task.get());
        //const std::type_info& t2 = typeid(WriteFileTask);

        if( typeid(*task.get()) == typeid(OpenFileTask) )
            doOpenFile( static_cast<const OpenFileTask*>(task.get()) );
        else if( typeid(*task.get()) == typeid(WriteFileTask) )
            doWriteFile( static_cast<const WriteFileTask*>(task.get()) );
        else if( typeid(*task.get()) == typeid(CloseFileTask) )
            doCloseFile( static_cast<const CloseFileTask*>(task.get()) );
    }
}

void AsyncFileProcessor::doOpenFile( const OpenFileTask* task )
{
    //TODO/IMPL
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
