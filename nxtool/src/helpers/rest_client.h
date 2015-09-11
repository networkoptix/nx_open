
#pragma once

#include <memory>
#include <functional>

#include <QUrlQuery>

#include <base/server_info.h>

namespace rtu
{
    /// @brief Sends REST requests to the specified server according to it availability 
    /// through the HTTP or multicast
    class RestClient
    {
    public:
        enum { kStandardTimeout = -1 };

        struct Request;
        typedef std::function<void (int errorCode)> ErrorCallback;
        typedef std::function<void (const QByteArray &data)> SuccessCallback;

    public:
        static void sendGet(const Request &request);

        static void sendPost(const Request &request
            , const QByteArray &data);
        
    private:
        RestClient();

        ~RestClient();

        static RestClient& instance();

    private:
        class Impl;
        typedef std::unique_ptr<Impl> ImplPtr;

        const ImplPtr m_impl;
    };

    struct RestClient::Request
    {
        BaseServerInfo target;
        QString password;

        QString path;
        QUrlQuery params;
        qint64 timeout;

        SuccessCallback replyCallback;
        ErrorCallback errorCallback;

        Request(const BaseServerInfo &initTarget
            , const QString &initPassword
            , const QString &initPath
            , const QUrlQuery &initParams
            , int initTimeout
            , const SuccessCallback &initReplyCallback
            , const ErrorCallback &initErrorCallback);

        Request(const ServerInfo &initServerInfo
            , const QString &initPath
            , const QUrlQuery &initParams
            , int initTimeout
            , const SuccessCallback &initReplyCallback
            , const ErrorCallback &initErrorCallback);
    };
};