
#pragma once

#include <QUuid>
#include <QFlag>
#include <QString>
#include <QDateTime>
#include <QMetaType>
#include <QScopedPointer>

namespace rtu
{

    enum class ServerFlag
    {
        kNoFlags
        , kAllowChangeDateTime
        , kAllowChangeInterfaceSettings
        , kIsFactory
    };
    
    Q_DECLARE_FLAGS(ServerFlags, ServerFlag)
    
    struct InterfaceInfo
    {
        QString name;
        
        QString ip;
        QString macAddress;
        QString mask;
        QString gateway;
        Qt::CheckState useDHCP;
        
        bool operator == (const InterfaceInfo &other);
    };
    typedef QList<InterfaceInfo> InterfaceInfoList;
    
    struct BaseServerInfo
    {
        QUuid id;
        ServerFlags flags;
        
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
        QDateTime dateTime;
        InterfaceInfoList interfaces;
    };
    
    class ServerInfo
    {
    public:
        ServerInfo();
        
        ServerInfo(const ServerInfo &other);
        
        ServerInfo(const BaseServerInfo &baseInfo);
        
        ~ServerInfo();
        
        ServerInfo &operator = (ServerInfo other);
        
    public:
        const BaseServerInfo &baseInfo() const;
        
        void setBaseInfo(const BaseServerInfo &baseInfo);
        
        ///
        
        bool hasExtraInfo() const;
        
        const ExtraServerInfo &extraInfo() const;
        
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

