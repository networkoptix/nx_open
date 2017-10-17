/**********************************************************
* 23 sep 2013
* a.kolesnikov
***********************************************************/

#include "get_file_operation.h"

#ifndef _WIN32
#include <sys/stat.h>
#endif

#include <functional>

#include <nx/utils/log/log.h>
#include <nx/utils/gzip/gzip_uncompressor.h>
#include <utils/fs/async_file_processor.h>
#include <nx/utils/byte_stream/custom_output_stream.h>

#include <applauncher_app_info.h>
#include "applauncher_process.h"


namespace detail
{
    GetFileOperation::GetFileOperation(
        int _id,
        const nx::utils::Url& baseUrl,
        const QString& filePath,
        const QString& localDirPath,
        const QString& hashTypeName,
        qint64 remoteFileSize,
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
        m_responseReceivedCalled( false ),
        m_fileClosePending( false )
    {
        if( remoteFileSize >= 0 )
            m_remoteFileSize = remoteFileSize;

        if( entryPath.endsWith(".gz") )
            m_filePath = entryPath.mid( 0, entryPath.size()-(sizeof(".gz")-1) );
        else
            m_filePath = entryPath;
    }

    GetFileOperation::~GetFileOperation()
    {
        if( !m_httpClient )
            return;

        m_httpClient->pleaseStopSync();
    }

    void GetFileOperation::pleaseStop()
    {
        {
            std::unique_lock<std::mutex> lk( m_mutex );

            if( m_state >= State::sInterrupted )
                return;

            m_state = State::sInterrupted;
            setResult( ResultCode::interrupted );
        }

        if( m_httpClient )
        {
            m_httpClient->pleaseStopSync();
            m_httpClient.reset();
        }

        {
            std::unique_lock<std::mutex> lk( m_mutex );

            if( !m_fileWritePending && m_outFile )
            {
                m_fileDataProcessor->flush();
                if( !m_fileClosePending )
                {
                    m_outFile->closeAsync( this );
                    m_fileClosePending = true;
                }
            }
        }
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
            //m_totalBytesWritten );
            m_totalBytesDownloaded );

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
            m_fileDataProcessor->flush();
            if( !m_fileClosePending )
            {
                m_outFile->closeAsync( this );
                m_fileClosePending = true;
            }
            return;
        }

        const nx_http::BufferType& partialMsgBody = m_httpClient->fetchMessageBodyBuffer();
        if( !partialMsgBody.isEmpty() )
        {
            m_totalBytesDownloaded += partialMsgBody.size();
            m_fileDataProcessor->processData( partialMsgBody );
            m_fileWritePending = 1;
        }
        else if( m_state == State::sWaitingForWriteToFinish )
        {
            m_fileDataProcessor->flush();
            if( !m_fileClosePending )
            {
                m_outFile->closeAsync( this );
                m_fileClosePending = true;
            }
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
        NX_LOG( QString::fromLatin1("RDirSyncher. GetFileOperation done. File %1, bytes written %2, result %3").
            arg(entryPath).arg(m_totalBytesWritten).arg(ResultCode::toString(result())), cl_logDEBUG2 );

#ifndef _WIN32
        //setting execution rights to file
        if (m_filePath.endsWith(QnApplauncherAppInfo::clientBinaryName()))
            chmod( (m_localDirPath + "/" + m_filePath).toUtf8().constData(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH );
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

                m_fileDataProcessor->processData( partialMsgBody );
                m_fileWritePending = 1;
                break;
            }

            case State::sInterrupted:
                return;

            default:
                NX_ASSERT( false );
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

                m_httpClient->pleaseStopSync();
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
            {
                if( httpClient->response()->statusLine.statusCode != nx_http::StatusCode::ok )
                {
                    setResult( ResultCode::downloadFailure );
                    setErrorText( httpClient->response()->statusLine.reasonPhrase );
                    m_httpClient->pleaseStopSync();
                    m_state = State::sInterrupted;
                    lk.unlock();
                    m_handler->operationDone( shared_from_this() );
                    return;
                }

                //opening file
                m_outFile.reset( new QnFile( m_localDirPath + "/" + m_filePath ) );
                if( !m_outFile->open( QIODevice::WriteOnly ) )  //TODO/IMPL have to do that asynchronously
                {
                    setResult( ResultCode::writeFailure );
                    setErrorText( SystemError::getLastOSErrorText() );
                    m_httpClient->pleaseStopSync();
                    m_state = State::sInterrupted;
                    lk.unlock();
                    m_handler->operationDone( shared_from_this() );
                    return;
                }

                using namespace std::placeholders;
                auto func = std::bind( &QnFile::writeAsync, m_outFile, _1, this );
                m_fileDataProcessor = std::make_shared<nx::utils::bstream::CustomOutputStream<decltype(func)>>( func );
                if( entryPath.endsWith(".gz") )
                    m_fileDataProcessor = std::make_shared<nx::utils::bstream::gzip::Uncompressor>(m_fileDataProcessor);
                break;
            }

            default:
                NX_ASSERT( false );
                break;
        }
    }

    void GetFileOperation::onSomeMessageBodyAvailable( nx_http::AsyncHttpClientPtr httpClient )
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        NX_ASSERT( m_httpClient == httpClient );
        onSomeMessageBodyAvailableNonSafe();
    }

    void GetFileOperation::onHttpDone( nx_http::AsyncHttpClientPtr httpClient )
    {
        NX_LOG( QString::fromLatin1("GetFileOperation::onHttpDone. file %1, state %2. http result %3").
            arg(entryPath).arg((int)m_state).arg(!httpClient->failed()), cl_logDEBUG2 );

        switch( m_state )
        {
            case State::sCheckingLocalFile:
                if( !httpClient->failed() )
                    return;

                {
                    std::unique_lock<std::mutex> lk( m_mutex );
                    setResult( ResultCode::downloadFailure );
                    setErrorText( httpClient->response() ? httpClient->response()->statusLine.reasonPhrase : "FAILURE" );    //TODO proper error text
                    m_state = State::sFinished;
                }
                NX_LOG( QString::fromLatin1("RDirSyncher. GetFileOperation::onHttpDone. Failed to get file %1 size").arg(entryPath), cl_logDEBUG2 );
                m_handler->operationDone( shared_from_this() );
                return;

            case State::sDownloadingFile:
                break;

            default:
                return;
        }

        std::unique_lock<std::mutex> lk( m_mutex );

        NX_ASSERT( m_httpClient == httpClient );

        //check message body download result
        if( httpClient->failed() )
        {
            //downloading has been interrupted unexpectedly
            setResult( ResultCode::downloadFailure );
            setErrorText( httpClient->response() ? httpClient->response()->statusLine.reasonPhrase : "FAILURE" );    //TODO proper error text

            if( !m_outFile.get() )  // TODO: #ak this condition should be replaced with one more state
            {
                m_state = State::sFinished;
                lk.unlock();
                m_handler->operationDone( shared_from_this() );
                return;
            }
        }
        else
        {
            onSomeMessageBodyAvailableNonSafe();
        }

        //waiting for pending file write to complete
        m_state = State::sWaitingForWriteToFinish;
        if( m_fileWritePending )
            return;

        m_fileDataProcessor->flush();
        if( !m_fileClosePending )
        {
            m_outFile->closeAsync( this );
            m_fileClosePending = true;
        }
    }

    bool GetFileOperation::startAsyncFilePresenceCheck()
    {
        m_state = State::sCheckingLocalFile;

        //checking file presense
        using namespace std::placeholders;
        if( !AsyncFileProcessor::instance()->statAsync(
                m_localDirPath + "/" + m_filePath,
                std::bind( std::mem_fn(&GetFileOperation::asyncStatDone), this, _1, _2 ) ) )
            return false;

        if( m_remoteFileSize )              //remote file size is already known
            return true;

        if( entryPath.endsWith(".gz") )     //remote file is compressed, compressed size is of no interest to us
        {
            // TODO: #ak can check last 4 bytes of remote file to get uncompressed file size
            m_remoteFileSize = -1;
            return true;
        }

        //starting async http request to get remote file size
        nx::utils::Url downloadUrl = baseUrl;
        downloadUrl.setPath( downloadUrl.path() + "/" + entryPath );
        m_httpClient = nx_http::AsyncHttpClient::create();
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
        m_httpClient->doGet( downloadUrl );

        return true;
    }

    void GetFileOperation::checkIfFileDownloadRequired()
    {
        NX_LOG( QString::fromLatin1("GetFileOperation::checkIfFileDownloadRequired. file %1").arg(entryPath), cl_logDEBUG2 );

        NX_ASSERT( m_localFileSize && m_remoteFileSize );
        NX_ASSERT( m_state == State::sCheckingLocalFile );

        if( m_localFileSize.get() >= 0 && m_localFileSize.get() == m_remoteFileSize.get() )
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

        nx::utils::Url downloadUrl = baseUrl;
        downloadUrl.setPath( downloadUrl.path() + "/" + entryPath );

        m_httpClient = nx_http::AsyncHttpClient::create();
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
        m_httpClient->doGet( downloadUrl );

        return true;
    }
}
