
#pragma once

#include <memory>
#include <functional>

#include <QUrlQuery>

#include <base/server_info.h>

namespace rtu
{
    // TODO mike: Convert to a singleton.
    /// @brief Sends REST requests to the specified server according to it availability 
    /// through the HTTP or multicast
    class RestClient : public QObject
    {
        Q_OBJECT
    public:
        enum class RequestResult;
        struct Request;

        // TODO mike: Add params: enum class HttpAccessType {tcp, udp, noChange}.
        typedef std::function<void (RequestResult errorCode)> ErrorCallback;
        typedef std::function<void (const QByteArray &data)> SuccessCallback;

    public:
        static void sendGet(const Request &request);

        static void sendPost(const Request &request
            , const QByteArray &data);
        
        static const QString &defaultAdminUserName();

        static const QStringList &defaultAdminPasswords();

        static RestClient *instance();

    signals:
        /// Emits when command has failed by http, but successfully executed by multicast
        void accessMethodChanged(const QUuid &id    
            , bool byHttp); /// TODO mike: change "byHttp" to enum with 2 values.

    private:
        RestClient();

        ~RestClient();

    private:
        class Impl;
        typedef std::unique_ptr<Impl> ImplPtr;

        static Impl& implInstance();

        const ImplPtr m_impl;
    };

    enum class RestClient::RequestResult
    {
        kSuccess,
        kRequestTimeout,
        kUnauthorized,
        kUnspecified,
    };

    struct RestClient::Request
    {
        BaseServerInfoPtr target;
        QString password;

        QString path;
        QUrlQuery params;
        qint64 timeoutMs;

        SuccessCallback replyCallback;
        ErrorCallback errorCallback;

        Request(const BaseServerInfoPtr &initTarget
            , const QString &initPassword
            , const QString &initPath
            , const QUrlQuery &initParams
            , int initTimeout
            , const SuccessCallback &initReplyCallback
            , const ErrorCallback &initErrorCallback);
    };
};