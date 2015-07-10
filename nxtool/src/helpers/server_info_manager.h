
#pragma once

#include <QObject>

#include <base/types.h>

namespace rtu
{
    struct BaseServerInfo;
    struct ExtraServerInfo;
    
    class ServerInfoManager : public QObject
    {
        Q_OBJECT
        
    public:
        ServerInfoManager(QObject *parent = nullptr);
        
        virtual ~ServerInfoManager();
       
        void loginToServer(const BaseServerInfo &info);
        
        void loginToServer(const BaseServerInfo &info
            , const QString &password);

        ///

        void updateServerInfos(const ServerInfoContainer &servers);

    signals:
        void loginOperationFailed(const QUuid &id);
        
        void loginOperationSuccessfull(const QUuid &id
            , const ExtraServerInfo &extraInfo
            , const QString &host);

        void serverExtraInfoUpdated(const QUuid &id
            , const ExtraServerInfo &extraInfo
            , const QString &host);

        void serverExtraInfoUpdateFailed(const QUuid &id);

    private:
        class Impl;
        Impl * const m_impl;        
    };
}
