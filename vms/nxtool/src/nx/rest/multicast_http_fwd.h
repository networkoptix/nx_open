#pragma once

#include <functional>
#include <QUuid>
#include <QUrl>
#include <QAuthenticator>
#include <QQueue>

namespace nx {
namespace rest {
namespace multicastHttp {

enum class ResultCode
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

typedef std::function<void(const QUuid& requestId, ResultCode errCode, const Response& response)> ResponseCallback;
typedef std::function<void(const QUuid& requestId, const QUuid& clientId, const Request& request)> RequestCallback;

} // namespace multicastHttp
} // namespace rest
} // namespace nx
