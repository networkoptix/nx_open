#ifndef __MULTICAST_HTTP_FWD_H__
#define __MULTICAST_HTTP_FWD_H__

namespace QnMulticast
{
    enum class ErrCode
    {
        ok           = 0,
        timeout      = 1,
        networkIssue = 2,
        unknown
    };
    struct Request
    {
        /** Server GUID */
        QUuid serverId;
        /** HTTP request. Only URL path and query params are used.*/
        QUrl url;
        /** Credentials - login and password */
        QAuthenticator auth;

        /** Extra http headers. user-agent for example */
        QList<QPair<QString, QString>> extraHttpHeaders;

        /** request message body. Usually It stay empty for GET requests */
        QByteArray messageBody;
    };

    struct Response
    {
        /** Server GUID */
        QUuid serverId;

        Response(): httpResult(0) {}
        /** HTTP result code received from server( code 200 means 'no error'). */
        int httpResult;
        /** Response content type */
        QByteArray contentType;
        /** Result data */
        QByteArray messageBody;
    };
    typedef std::function<void(const QUuid& requestId, ErrCode errCode, const Response& response)> ResponseCallback;
}

#endif // __MULTICAST_HTTP_FWD_H__
