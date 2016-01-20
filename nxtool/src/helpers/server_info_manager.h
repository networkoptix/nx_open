
#pragma once

#include <functional>

#include <QObject>

#include <nx/mediaserver/api/client.h>

namespace rtu
{
    class ServerInfoManager : public QObject
    {
        Q_OBJECT

    public:
        typedef nx::mediaserver::api::Client Client;
        typedef nx::mediaserver::api::BaseServerInfo BaseServerInfo;
        typedef nx::mediaserver::api::ExtraServerInfo ExtraServerInfo;
        typedef nx::mediaserver::api::ServerInfoContainer ServerInfoContainer;

        typedef std::function<void (const QUuid &id, const ExtraServerInfo &extraInfo
            , const QString &host)> SuccessfulCallback;
        typedef std::function<void (const QUuid &id
            , Client::ResultCode errorCode)> FailedCallback;

        ServerInfoManager(QObject *parent = nullptr);

        virtual ~ServerInfoManager();

        void loginToServer(const BaseServerInfo &info
            , const SuccessfulCallback &success
            , const FailedCallback &failed);

        void loginToServer(const BaseServerInfo &info
            , const QString &password
            , const SuccessfulCallback &success
            , const FailedCallback &failed);

        ///

        void updateServerInfos(const ServerInfoContainer &servers
            , const SuccessfulCallback &success
            , const FailedCallback &failed);

    private:
        class Impl;
        Impl * const m_impl;
    };
}
