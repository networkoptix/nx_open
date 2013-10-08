/**********************************************************
* 23 sep 2013
* a.kolesnikov
***********************************************************/

#include "get_file_operation.h"

#include <utils/common/log.h>
#include <utils/network/http/asynchttpclient.h>


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
        m_httpClient( nullptr ),
        m_state( State::sInit ),
        m_localDirPath( localDirPath ),
        m_hashTypeName( hashTypeName ),
        m_fileWritePending( 0 ),
        m_totalBytesDownloaded( 0 ),
        m_totalBytesWritten( 0 )
    {
        m_httpClient = new nx_http::AsyncHttpClient();
        connect(
            m_httpClient, SIGNAL(responseReceived(nx_http::AsyncHttpClient*)),
            this, SLOT(onResponseReceived(nx_http::AsyncHttpClient*)),
            Qt::DirectConnection );
        connect(
            m_httpClient, SIGNAL(someMessageBodyAvailable(nx_http::AsyncHttpClient*)),
            this, SLOT(onSomeMessageBodyAvailable(nx_http::AsyncHttpClient*)),
            Qt::DirectConnection );
        connect(
            m_httpClient, SIGNAL(done(nx_http::AsyncHttpClient*)),
            this, SLOT(onHttpDone(nx_http::AsyncHttpClient*)),
            Qt::DirectConnection );
    }

    GetFileOperation::~GetFileOperation()
    {
        if( !m_httpClient )
            return;

        m_httpClient->terminate();
        m_httpClient->scheduleForRemoval();
        m_httpClient = nullptr;
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
            m_httpClient->scheduleForRemoval();
            m_httpClient = nullptr;
        }

        if( !m_fileWritePending )
            m_outFile->closeAsync( this );
    }

    //!Implementation of RDirSynchronizationOperation::startAsync
    bool GetFileOperation::startAsync()
    {
        //TODO/IMPL file hash check

        m_state = State::sDownloadingFile;
        return startFileDownload();
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
        m_handler->operationDone( shared_from_this() );
    }

    void GetFileOperation::onSomeMessageBodyAvailableNonSafe()
    {
        switch( m_state )
        {
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

    void GetFileOperation::onResponseReceived( nx_http::AsyncHttpClient* httpClient )
    {
        std::unique_lock<std::mutex> lk( m_mutex );

        switch( m_state )
        {
            case State::sDownloadingFile:
                if( (httpClient->response()->statusLine.statusCode != nx_http::StatusCode::ok) ||
                    !httpClient->startReadMessageBody() )
                {
                    setResult( ResultCode::downloadFailure );
                    setErrorText( httpClient->response()->statusLine.reasonPhrase );
                    m_httpClient->terminate();
                    m_httpClient->scheduleForRemoval();
                    m_httpClient = nullptr;
                    m_state = State::sInterrupted;
                    m_outFile->closeAsync( this );
                    return;
                }
                break;

            default:
                assert( false );
                break;
        }
    }

    void GetFileOperation::onSomeMessageBodyAvailable( nx_http::AsyncHttpClient* httpClient )
    {
        std::unique_lock<std::mutex> lk( m_mutex );
        assert( m_httpClient == httpClient );
        onSomeMessageBodyAvailableNonSafe();
    }

    void GetFileOperation::onHttpDone( nx_http::AsyncHttpClient* httpClient )
    {
        std::unique_lock<std::mutex> lk( m_mutex );

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

    bool GetFileOperation::startFileDownload()
    {
        m_outFile.reset( new QnFile( m_localDirPath + "/" + entryPath ) );

        //opening file
        if( !m_outFile->open( QIODevice::WriteOnly ) )  //TODO/IMPL have to do that asynchronously
            return false;

        QUrl downloadUrl = baseUrl;
        downloadUrl.setPath( downloadUrl.path() + "/" + entryPath );
        if( !m_httpClient->doGet( downloadUrl ) )
        {
            m_outFile->close();
            return false;
        }

        return true;
    }
}
