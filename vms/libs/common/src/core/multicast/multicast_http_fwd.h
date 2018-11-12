#pragma once

#include <QUuid>
#include <QUrl>
#include <QtNetwork/QAuthenticator>
#include <QQueue>
#include <functional>

namespace QnMulticast
{
    enum class ErrCode
    {
        ok           = 0,
        timeout      = 1,
        networkIssue = 2,
        unknown
    };

    typedef QPair<QString, QString> Header;
    struct Message
    {
        /** Server GUID */
        QUuid serverId;

        /** Extra http headers. user-agent for example */
        QList<Header> headers;

        QByteArray contentType;

        /** request message body. Usually It stay empty for GET requests */
        QByteArray messageBody;
    };

    struct Request: public Message
    {
        /** HTTP method (GET, POST, e.t.c) */
        QString method;

        /** HTTP request. Only URL path and query params are used.*/
        QUrl url;

        /** Credentials - login and password */
        QAuthenticator auth;
    };

    struct Response: public Message
    {
        Response(): Message(), httpResult(0) {}

        /** HTTP result code received from server( code 200 means 'no error'). */
        int httpResult;
    };
    typedef std::function<void(const QUuid& requestId, ErrCode errCode, const Response& response)> ResponseCallback;
    //typedef std::function<void(const QUuid& requestId, const QUuid& clientId, const QByteArray& httpData)> RequestCallback;
    typedef std::function<void(const QUuid& requestId, const QUuid& clientId, const Request& request)> RequestCallback;
}
