
#pragma once

#include <functional>

#include <QObject>

#include <nx/vms/server/api/client.h>

namespace rtu
{
    class ServerInfoManager : public QObject
    {
        Q_OBJECT

    public:
        typedef nx::vms::server::api::Client Client;
        typedef nx::vms::server::api::BaseServerInfo BaseServerInfo;
        typedef nx::vms::server::api::ExtraServerInfo ExtraServerInfo;
        typedef nx::vms::server::api::ServerInfoContainer ServerInfoContainer;

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
