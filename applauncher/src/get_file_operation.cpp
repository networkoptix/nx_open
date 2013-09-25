/**********************************************************
* 23 sep 2013
* a.kolesnikov
***********************************************************/

#include "get_file_operation.h"

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
        m_localDirPath( localDirPath ),
        m_hashTypeName( hashTypeName ),
        m_httpClient( nullptr ),
        m_state( State::sInit )
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

    void GetFileOperation::pleaseStop()
    {
        //TODO/IMPL
    }

    //!Implementation of RDirSynchronizationOperation::startAsync
    bool GetFileOperation::startAsync()
    {
        //TODO/IMPL file hash check

        m_state = State::sDownloadingFile;
        return onEnteredDownloadingFile();
    }

    void GetFileOperation::onResponseReceived( nx_http::AsyncHttpClient* httpClient )
    {
        //switch( m_state )
        //{
        //    case State::sDownloadingFile:
        //}
    }

    void GetFileOperation::onSomeMessageBodyAvailable( nx_http::AsyncHttpClient* httpClient )
    {
        //TODO/IMPL
    }

    void GetFileOperation::onHttpDone( nx_http::AsyncHttpClient* httpClient )
    {
        //TODO/IMPL
    }

    bool GetFileOperation::onEnteredDownloadingFile()
    {
        QUrl downloadUrl = baseUrl;
        downloadUrl.setPath( downloadUrl.path() + "/" + entryPath );
        if( !m_httpClient->doGet( downloadUrl ) )
            return false;

        //TODO/IMPL opening file

        return true;
    }
}
