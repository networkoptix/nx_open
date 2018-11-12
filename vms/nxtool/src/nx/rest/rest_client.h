#pragma once

#include <memory>
#include <functional>

#include <QString>
#include <QUuid>
#include <QUrlQuery>

namespace nx {
namespace rest {

/**
 * Sends REST requests to the specified server according to its availability 
 * through TCP HTTP or UDP multicast.
 */
class RestClient
{
public:
    enum class ResultCode
    {
        kSuccess,
        kRequestTimeout,
        kUnauthorized,
        kUnspecified,
    };

    enum class NewHttpAccessMethod
    {
        kTcp,
        kUdp,
        kUnchanged,
    };

    enum class HttpAccessMethod
    {
        kTcp,
        kUdp,
    };

    struct Request;

    typedef std::function<void (ResultCode resultCode, NewHttpAccessMethod newHttpAccessMethod)> ErrorCallback;
    typedef std::function<void (const QByteArray &data, NewHttpAccessMethod newHttpAccessMethod)> SuccessCallback;

    RestClient(const QString& userAgent, const QString& userName);
    virtual ~RestClient();

    void sendGet(const Request &request);

    void sendPost(const Request &request, const QByteArray &data);
        
private:
    class Impl;
    const std::unique_ptr<Impl> m_impl;
};

struct RestClient::Request
{
    QUuid serverId;
    QString hostAddress;
    int port;
    HttpAccessMethod httpAccessMethod;

    QString password;

    QString path;
    QUrlQuery params;
    qint64 timeoutMs;

    SuccessCallback successCallback;
    ErrorCallback errorCallback;

    Request(
        QUuid initServerId,
        const QString &initHostAddress,
        int initPort,
        HttpAccessMethod initHttpAccessMethod,
        const QString &initPassword,
        const QString &initPath,
        const QUrlQuery &initParams,
        qint64 initTimeoutMs,
        const RestClient::SuccessCallback &initSuccessCallback,
        const RestClient::ErrorCallback &initErrorCallback);
};

} // namespace rest
} // namespace nx
