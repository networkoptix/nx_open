/**********************************************************
* 23 sep 2013
* a.kolesnikov
***********************************************************/

#include "get_file_operation.h"

#ifndef _WIN32
#include <sys/stat.h>
#endif

#include <functional>

#include <utils/common/log.h>
#include <utils/fs/async_file_processor.h>

#include "applauncher_process.h"


namespace detail
{
    GetFileOperation::GetFileOperation(
        int _id,
        const QUrl& baseUrl,
        const QString& filePath,
        const QString& localDirPath,
        const QString& hashTypeName,
        AbstractRDirSynchronizationEventHandler* _handler )
    :
        RDirSynchronizationOperation(
            RSyncOperationType::getFile,
            _id,
            baseUrl,
            filePath,
            _handler ),
        m_state( State::sInit ),
        m_localDirPath( localDirPath ),
        m_hashTypeName( hashTypeName ),
        m_fileWritePending( 0 ),
        m_totalBytesDownloaded( 0 ),
        m_totalBytesWritten( 0 ),
        m_responseReceivedCalled( false )
    {
    }

    GetFileOperation::~GetFileOperation()
    {
        if( !m_httpClient )
            return;

        m_httpClient->terminate();
    }

    void GetFileOperation::pleaseStop()
    {
        std::unique_lock<std::mutex> lk( m_mutex );

        if( m_state >= State::sInterrupted )
            return;

        m_state = State::sInterrupted;
        setResult( ResultCode::interrupted );
        if( m_httpClient )
        {
            m_httpClient->terminate();
            m_httpClient.reset();
        }

        if( !m_fileWritePending )
            m_outFile->closeAsync( this );
    }

    //!Implementation of RDirSynchronizationOperation::startAsync
    bool GetFileOperation::startAsync()
    {
        //TODO/IMPL file hash check

        return startAsyncFilePresenceCheck();
    }

    void GetFileOperation::asyncStatDone( SystemError::ErrorCode errorCode, qint64 fileSize )
    {
        NX_LOG( QString::fromLatin1("GetFileOperation::asyncStatDone. file %1, result %2").arg(entryPath).arg(SystemError::toString(errorCode)), cl_logDEBUG2 );

        std::unique_lock<std::mutex> lk( m_mutex );

        if( errorCode == SystemError::noError )
            m_localFileSize = fileSize;
        else    //in case of other error trying to download file anyway
            m_localFileSize = -1;

        if( m_remoteFileSize )
        {
            lk.unlock();
            checkIfFileDownloadRequired();
        }
    }

    void GetFileOperation::onAsyncWriteFinished(
        const std::shared_ptr<QnFile>& /*file*/,
        uint64_t bytesWritten,
        SystemError::ErrorCode errorCode )
    {
        m_totalBytesWritten += bytesWritten;
        m_handler->downloadProgress(
            shared_from_this(),
            -1,
            m_totalBytesWritten );

        std::unique_lock<std::mutex> lk( m_mutex );

        m_fileWritePending = 0;

        if( errorCode != SystemError::noError )
        {
            setResult( ResultCode::writeFailure );
            m_state = State::sInterrupted;
        }

        // if have something to write and not interrupted, writing
        if( !m_httpClient || m_state >= State::sInterrupted )
        {
            //closing file
            m_outFile->closeAsync( this );
            return;
        }

        const nx_http::BufferType& partialMsgBody = m_httpClient->fetchMessageBodyBuffer();
        if( !partialMsgBody.isEmpty() )
        {
            m_totalBytesDownloaded += partialMsgBody.size();
            m_outFile->writeAsync( partialMsgBody, this );
            m_fileWritePending = 1;
        }
        else if( m_state == State::sWaitingForWriteToFinish )
        {
            //read all source data
            m_outFile->closeAsync( this );
        }
    }

    void GetFileOperation::onAsyncCloseFinished(
        const std::shared_ptr<QnFile>& /*file*/,
        SystemError::ErrorCode /*errorCode*/ )
    {
        {
            std::unique_lock<std::mutex> lk( m_mutex );
            m_state = State::sFinished;
        }
        NX_LOG( QString::fromLatin1("RDirSyncher. GetFileOperation done. File %1,  bytes written %2").arg(entryPath).arg(m_totalBytesWritten), cl_logDEBUG2 );

#ifndef _WIN32
        //setting execution rights to file
        if( entryPath.endsWith(QN_CLIENT_EXECUTABLE_NAME) )
            chmod( (m_localDirPath + "/" + entryPath).toUtf8().constData(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH );
#endif

        m_handler->operationDone( shared_from_this() );
    }

    void GetFileOperation::onSomeMessageBodyAvailableNonSafe()
    {
        switch( m_state )
        {
            case State::sCheckingLocalFile:
                return; //ignoring message body in this state

            case State::sDownloadingFile:
            {
                if( m_fileWritePending )
                    return; //not reading message body since previous message body buffer has not been written yet

                const nx_http::BufferType& partialMsgBody = m_httpClient->fetchMessageBodyBuffer();
                if( partialMsgBody.isEmpty() )
                    return;

                m_totalBytesDownloaded += partialMsgBody.size();

                m_outFile->writeAsync( partialMsgBody, this );
                m_fileWritePending = 1;
                break;
            }

            default:
                assert( false );
                break;
        }
    }

    void GetFileOperation::onResponseReceived( nx_http::AsyncHttpClientPtr httpClient )
    {
        NX_LOG( QString::fromLatin1("GetFileOperation::onResponseReceived. file %1").arg(entryPath), cl_logDEBUG2 );

        m_responseReceivedCalled = true;

        std::unique_lock<std::mutex> lk( m_mutex );

        switch( m_state )
        {
            case State::sCheckingLocalFile:
            {
                const nx_http::StringType& contentLengthStr = nx_http::getHeaderValue( httpClient->response()->headers, "Content-Length" );
                if( httpClient->response()->statusLine.statusCode != nx_http::StatusCode::ok ||
                    contentLengthStr.isEmpty() )
                {
                    //assuming file size is unknown and proceeding with download...
                    m_remoteFileSize = -1;
                }
                else
                {
                    m_remoteFileSize = contentLengthStr.toLongLong();
                }

                m_httpClient->terminate();
                m_httpClient.reset();

                if( m_localFileSize )
                {
                    lk.unlock();
                    checkIfFileDownloadRequired();
                    return;
                }
                break;
            }

            case State::sDownloadingFile:
                if( httpClient->response()->statusLine.statusCode != nx_http::StatusCode::ok )
                {
                    setResult( ResultCode::downloadFailure );
                    setErrorText( httpClient->response()->statusLine.reasonPhrase );
                    m_httpClient->terminate();
                    m_state = State::sInterrupted;
                    lk.unlock();
                    m_handler->operationDone( shared_from_this() );
                    return;
                }

                //opening file
                m_outFile.reset( new QnFile( m_localDirPath + "/" + entryPath ) );
                if( !m_outFile->open( QIODevice::WriteOnly ) )  //TODO/IMPL have to do that asynchronously
                {
                    setResult( ResultCode::writeFailure );
                    setErrorText( SystemError::getLastOSErrorText() );
                    m_httpClient->terminate();
                    m_state = State::sInterrupted;
                    lk.unlock();
                    m_handler->operationDone( shared_from_this() );
                    return;
                }
                break;

            default:
                assert( false );
                break;
        }
    }

    void GetFileOperation::onSomeMessageBodyAvailable( nx_http::AsyncHttpClientPtr httpClient )
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        assert( m_httpClient == httpClient );
        onSomeMessageBodyAvailableNonSafe();
    }

    void GetFileOperation::onHttpDone( nx_http::AsyncHttpClientPtr httpClient )
    {
        NX_LOG( QString::fromLatin1("GetFileOperation::onHttpDone. file %1").arg(entryPath), cl_logDEBUG2 );

        //TODO/IMPL in case of error we'll be here in state State::sCheckingLocalFile

        std::unique_lock<std::mutex> lk( m_mutex );

        if( m_state != State::sDownloadingFile )
            return;

        assert( m_httpClient == httpClient );

        //check message body download result
        if( httpClient->failed() )
        {
            //downloading has been interrupted unexpectedly
            setResult( ResultCode::downloadFailure );
            setErrorText( httpClient->response()->statusLine.reasonPhrase );    //TODO proper error text
        }
        else
        {
            onSomeMessageBodyAvailableNonSafe();
        }

        //waiting for pending file write to complete
        m_state = State::sWaitingForWriteToFinish;
        if( m_fileWritePending )
            return;

        m_outFile->closeAsync( this );
    }

    bool GetFileOperation::startAsyncFilePresenceCheck()
    {
        //m_state = State::sDownloadingFile;
        m_state = State::sCheckingLocalFile;

        //checking file presense
        using namespace std::placeholders;
        if( !AsyncFileProcessor::instance()->statAsync(
                m_localDirPath + "/" + entryPath,
                std::bind( std::mem_fn(&GetFileOperation::asyncStatDone), this, _1, _2 ) ) )
            return false;

        //starting async http request to get remote file size
        QUrl downloadUrl = baseUrl;
        downloadUrl.setPath( downloadUrl.path() + "/" + entryPath );
        m_httpClient.reset( new nx_http::AsyncHttpClient() );
        connect(
            m_httpClient.get(), &nx_http::AsyncHttpClient::responseReceived,
            this, &GetFileOperation::onResponseReceived,
            Qt::DirectConnection );
        connect(
            m_httpClient.get(), &nx_http::AsyncHttpClient::someMessageBodyAvailable,
            this, &GetFileOperation::onSomeMessageBodyAvailable,
            Qt::DirectConnection );
        connect(
            m_httpClient.get(), &nx_http::AsyncHttpClient::done,
            this, &GetFileOperation::onHttpDone,
            Qt::DirectConnection );
        m_httpClient->setUseCompression( false );   //not using compression for server to report Content-Length
        if( !m_httpClient->doGet( downloadUrl ) )
        {
            //TODO/IMPL we must return false here and cancel async stat. But for the moment cancellation is not implemented, so assert is here...
            assert( false );
        }

        return true;
    }

    void GetFileOperation::checkIfFileDownloadRequired()
    {
        NX_LOG( QString::fromLatin1("GetFileOperation::checkIfFileDownloadRequired. file %1").arg(entryPath), cl_logDEBUG2 );

        assert( m_localFileSize && m_remoteFileSize );
        assert( m_state == State::sCheckingLocalFile );

        if( m_localFileSize >= 0 && m_localFileSize == m_remoteFileSize )
        {
            //file valid, no need to download
            {
                std::unique_lock<std::mutex> lk( m_mutex );
                m_state = State::sFinished;
            }
            NX_LOG( QString::fromLatin1("RDirSyncher. GetFileOperation done. File %1 with size %2 already exists, no need to download").arg(entryPath).arg(m_localFileSize.get()), cl_logDEBUG2 );
            m_handler->operationDone( shared_from_this() );
            return;
        }

        if( m_localFileSize == (qint64)-1 )
        {
            NX_LOG( QString::fromLatin1("RDirSyncher. GetFileOperation. File %1 not found. Downloading...").arg(entryPath), cl_logDEBUG2 );
        }
        else
        {
            NX_LOG( QString::fromLatin1("RDirSyncher. GetFileOperation. File %1 not valid: local size %2, remote size %3. Downloading...").
                arg(entryPath).arg(m_localFileSize.get()).arg(m_remoteFileSize.get()), cl_logDEBUG2 );
        }

        startFileDownload();
    }

    bool GetFileOperation::startFileDownload()
    {
        {
            std::unique_lock<std::mutex> lk( m_mutex );
            m_state = State::sDownloadingFile;
        }

        QUrl downloadUrl = baseUrl;
        downloadUrl.setPath( downloadUrl.path() + "/" + entryPath );

        m_httpClient.reset( new nx_http::AsyncHttpClient() );
        connect(
            m_httpClient.get(), &nx_http::AsyncHttpClient::responseReceived,
            this, &GetFileOperation::onResponseReceived,
            Qt::DirectConnection );
        connect(
            m_httpClient.get(), &nx_http::AsyncHttpClient::someMessageBodyAvailable,
            this, &GetFileOperation::onSomeMessageBodyAvailable,
            Qt::DirectConnection );
        connect(
            m_httpClient.get(), &nx_http::AsyncHttpClient::done,
            this, &GetFileOperation::onHttpDone,
            Qt::DirectConnection );
        if( !m_httpClient->doGet( downloadUrl ) )
        {
            setResult( ResultCode::downloadFailure );
            setErrorText( SystemError::getLastOSErrorText() );
            {
                std::unique_lock<std::mutex> lk( m_mutex );
                m_state = State::sFinished;
            }
            m_handler->operationDone( shared_from_this() );
            return false;
        }

        return true;
    }
}
