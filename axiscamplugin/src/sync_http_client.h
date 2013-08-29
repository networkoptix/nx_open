/**********************************************************
* 18 apr 2013
* akolesnikov
***********************************************************/

#ifndef SYNC_HTTP_CLIENT_H
#define SYNC_HTTP_CLIENT_H

#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>


class SyncHttpClientDelegate;

//!Synchronous wrapper on top of QNetworkAccessManager
/*!
    Host and port can be specified at object construction, than \a get can accept only http path
    \note This class instance is not thread-safe but can be used in any thread except QNetworkAccessManager thread 
        (in contrast with QNetworkAccessManager which can be used in its thread only)
*/
class SyncHttpClient
{
public:
    static const int HTTP_OK = 200;
    static const int HTTP_BAD_REQUEST = 400;
    static const int HTTP_NOT_AUTHORIZED = 401;

    /*!
        \param networkAccessManager This object MUST not be freed earlier, than \a SyncHttpClient object
        \param defaultUrl Contains host and port to use if url passed to \a get does not contain host & port
        \param defaultPort Used as default if \a defaultUrl does not contain port
        \param defaultCredentials Used as default if \a defaultUrl does not contain user credentials
    */
    SyncHttpClient(
        QNetworkAccessManager* networkAccessManager,
        const QUrl& defaultUrl = QUrl(),
        int defaultPort = 0,
        const QAuthenticator& defaultCredentials = QAuthenticator() );
    SyncHttpClient(
        QNetworkAccessManager* networkAccessManager,
        const QString& defaultUrlStr,
        int defaultPort = 0,
        const QAuthenticator& defaultCredentials = QAuthenticator() );
    ~SyncHttpClient();

    //!Perform GET request
    /*!
        Blocks till valid response has been recevied or an error occured
        \param request
        \return In case of valid response received, returns \a QNetworkReply::NoError (but \a *httpStatusCode may contain some http error)
    */
    QNetworkReply::NetworkError get( const QNetworkRequest& request );
    //!Same as another \a get
    QNetworkReply::NetworkError get( const QUrl& requestUrl );
    //!Same as another \a get
    QNetworkReply::NetworkError get( const QString& requestUrl );
    //!Reads whole message body. May block
    QByteArray readWholeMessageBody();
    //!Holds http status code of prev request. Undefined, if get returned code different from \a QNetworkReply::NoError
    int statusCode() const;

private:
    QNetworkAccessManager* m_networkAccessManager;
    QUrl m_defaultUrl;
    SyncHttpClientDelegate* m_delegate;

    void init(
        int defaultPort,
        const QAuthenticator& defaultCredentials );
};

#endif  //SYNC_HTTP_CLIENT_H
