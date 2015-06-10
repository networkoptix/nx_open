
#pragma once

#include <QUuid>
#include <QFlag>
#include <QString>
#include <QDateTime>
#include <QTimeZone>
#include <QMetaType>
#include <QScopedPointer>

#include <base/constants.h>

namespace rtu
{

    struct InterfaceInfo
    {
        QString name;
        
        QString ip;
        QString macAddress;
        QString mask;
        QString gateway;
        Qt::CheckState useDHCP;
        
        bool operator == (const InterfaceInfo &other);

        InterfaceInfo();

        InterfaceInfo(bool initUseDHCP);

        InterfaceInfo(QString initName
            , QString initIp
            , QString initMacAddress
            , QString initMask
            , QString initGateway
            , Qt::CheckState initUseDHCP);

    };
    typedef QList<InterfaceInfo> InterfaceInfoList;
    
    struct BaseServerInfo
    {
        QUuid id;
        Constants::ServerFlags flags;
        
        QString name;
        QString systemName;
        QString hostAddress;
        int port;
    };
    
    bool operator == (const BaseServerInfo &first
        , const BaseServerInfo &second);
    
    bool operator != (const BaseServerInfo &first
        , const BaseServerInfo &second);
    
    struct ExtraServerInfo
    {
        QString password;
        QDateTime timestamp;
        QDateTime utcDateTime;
        QTimeZone timeZone;
        InterfaceInfoList interfaces;
        
        ExtraServerInfo();
        
        ExtraServerInfo(const QString &initPassword
            , const QDateTime &initTimestamp
            , const QDateTime &initUtcDateTime
            , const QTimeZone &initTimeZone
            , const InterfaceInfoList initInterfaces);
    };
    
    class ServerInfo
    {
    public:
        ServerInfo();
        
        ServerInfo(const ServerInfo &other);
        
        ServerInfo(const BaseServerInfo &baseInfo);
        
        ~ServerInfo();
        
        ServerInfo &operator = (const ServerInfo &other);
        
    public:
        const BaseServerInfo &baseInfo() const;
        
        BaseServerInfo &writableBaseInfo();
        
        void setBaseInfo(const BaseServerInfo &baseInfo);
        
        ///
        
        bool hasExtraInfo() const;
        
        const ExtraServerInfo &extraInfo() const;
        
        ExtraServerInfo &writableExtraInfo();
        
        void setExtraInfo(const ExtraServerInfo &extraInfo);
        
        void resetExtraInfo();
        
    private:
        bool operator == (const ServerInfo);
        
    private:
        typedef QScopedPointer<ExtraServerInfo> ExtraServerInfoPtr;
        
        BaseServerInfo m_base;
        ExtraServerInfoPtr m_extra;
    };
}

Q_DECLARE_METATYPE(rtu::InterfaceInfo)

