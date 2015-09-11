
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
        struct Request;

    public:
        static RestClient& instance();

        void sendGet(const Request &request);

        void sendPost(const Request &request
            , const QByteArray &data);
        
    private:
        RestClient();

        ~RestClient();

    private:
        class Impl;
        typedef std::unique_ptr<Impl> ImplPtr;

        const ImplPtr m_impl;
    };

    struct RestClient::Request
    {
        typedef std::function<void (int errorCode)> ErrorCallback;
        typedef std::function<void (const QByteArray &data)> ReplyCallback;

        BaseServerInfo target;

        QString path;
        QUrlQuery params;
        QString username;
        QString password;
        qint64 timeout;

        ErrorCallback errorCallback;
        ReplyCallback replyCallback;

        Request(BaseServerInfo initTarget
            , QString initPath
            , QUrlQuery initParams
            , QString initUsername
            , QString initPassword
            , int initTimeout
            , const ErrorCallback &initErrorCallback
            , const ReplyCallback &initReplyCallback);
    };
};