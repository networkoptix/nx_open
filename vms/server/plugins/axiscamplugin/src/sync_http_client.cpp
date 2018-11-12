/**********************************************************
* 18 apr 2013
* akolesnikov
***********************************************************/

#include "sync_http_client.h"

#include <cassert>

#include <QThread>

#include "sync_http_client_delegate.h"


SyncHttpClient::SyncHttpClient(
    QNetworkAccessManager* networkAccessManager,
    const QUrl& defaultUrl,
    int defaultPort,
    const QAuthenticator& defaultCredentials )
:
    m_networkAccessManager( networkAccessManager ),
    m_defaultUrl( defaultUrl ),
    m_delegate( new SyncHttpClientDelegate(networkAccessManager) )
{
    init( defaultPort, defaultCredentials );
}

SyncHttpClient::SyncHttpClient(
    QNetworkAccessManager* networkAccessManager,
    const QString& defaultUrlStr,
    int defaultPort,
    const QAuthenticator& defaultCredentials )
:
    m_networkAccessManager( networkAccessManager ),
    m_delegate( new SyncHttpClientDelegate(networkAccessManager) )
{
    m_defaultUrl = QUrl( defaultUrlStr.startsWith("/") ? defaultUrlStr : QString::fromLatin1("//%1").arg(defaultUrlStr) );

    init( defaultPort, defaultCredentials );
}

SyncHttpClient::~SyncHttpClient()
{
    m_delegate->deleteLater();
}

QNetworkReply::NetworkError SyncHttpClient::get( const QNetworkRequest& request )
{
    assert( QThread::currentThread() != m_networkAccessManager->thread() );

    QNetworkRequest requestCopy( request );
    QUrl requestUrlCopy = requestCopy.url();
    if( requestUrlCopy.scheme().isEmpty() )
        requestUrlCopy.setScheme( QLatin1String("http") );
    if( requestUrlCopy.host().isEmpty() )
        requestUrlCopy.setHost( m_defaultUrl.host() );
    if( !requestUrlCopy.port() )
        requestUrlCopy.setPort( m_defaultUrl.port() );
    if( requestUrlCopy.userName().isEmpty() )
        requestUrlCopy.setUserName( m_defaultUrl.userName() );
    if( requestUrlCopy.password().isEmpty() )
        requestUrlCopy.setPassword( m_defaultUrl.password() );
    requestCopy.setUrl( requestUrlCopy );

    //initiating async request
    //waiting for request completion
    m_delegate->get( requestCopy );
    return m_delegate->resultCode() == QNetworkReply::ContentAccessDenied || m_delegate->resultCode() == QNetworkReply::AuthenticationRequiredError
        ? QNetworkReply::NoError    //reporting access denied as http status code 401 to avoid confusion at a higher level
        : m_delegate->resultCode();
}

QNetworkReply::NetworkError SyncHttpClient::get( const QUrl& requestUrl )
{
    return get( QNetworkRequest(requestUrl) );
}

QNetworkReply::NetworkError SyncHttpClient::get( const QString& requestUrl )
{
    return get( QUrl(requestUrl) );
}

QByteArray SyncHttpClient::readWholeMessageBody()
{
    return m_delegate->readWholeMessageBody();
}

int SyncHttpClient::statusCode() const
{
    return m_delegate->resultCode() == QNetworkReply::ContentAccessDenied || m_delegate->resultCode() == QNetworkReply::AuthenticationRequiredError
        ? HTTP_NOT_AUTHORIZED    //reporting access denied as http status code 401 to avoid confusion at a higher level
        : m_delegate->statusCode();
}

void SyncHttpClient::init(
    int defaultPort,
    const QAuthenticator& defaultCredentials )
{
    if( !m_defaultUrl.port() )
        m_defaultUrl.setPort( defaultPort );
    if( m_defaultUrl.userName().isEmpty() )
        m_defaultUrl.setUserName( defaultCredentials.user() );
    if( m_defaultUrl.password().isEmpty() )
        m_defaultUrl.setPassword( defaultCredentials.password() );

    m_delegate->moveToThread( m_networkAccessManager->thread() );
}
