
#pragma once

#include <functional>

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

        typedef std::function<void (const QUuid &id, const ExtraServerInfo &extraInfo
            , const QString &host)> SuccessfulCallback;
        typedef std::function<void (const QUuid &id
            , RequestError errorCode)> FailedCallback;

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
