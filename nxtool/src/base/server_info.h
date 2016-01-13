
#pragma once

#include <memory>

#include <QVector>
#include <QList>
#include <QUuid>
#include <QFlag>
#include <QString>
#include <QDateTime>
#include <QTimeZone>
#include <QMetaType>
#include <QScopedPointer>

#include <base/constants.h>
#include <helpers/version_holder.h>

namespace rtu
{
    struct InterfaceInfo;
    struct BaseServerInfo;
    struct ExtraServerInfo;
    class ServerInfo;

    typedef QList<InterfaceInfo> InterfaceInfoList;
    typedef QVector<ServerInfo *> ServerInfoPtrContainer;
    typedef QVector<ServerInfo> ServerInfoContainer;
    typedef std::shared_ptr<ServerInfo> ServerInfoPtr;
    typedef std::shared_ptr<BaseServerInfo> BaseServerInfoPtr;

    struct InterfaceInfo
    {
        QString name;
        
        QString ip;
        QString macAddress;
        QString mask;
        QString gateway;
        QString dns;
        Qt::CheckState useDHCP;
        
        InterfaceInfo();

        InterfaceInfo(const QString &initName
            , const QString &initIp
            , const QString &initMacAddress
            , const QString &initMask
            , const QString &initGateway
            , const QString &initDns
            , Qt::CheckState initUseDHCP);

    };

    bool operator == (const InterfaceInfo &first
        , const InterfaceInfo &second);

    bool operator != (const InterfaceInfo &first
        , const InterfaceInfo &second);

    struct BaseServerInfo
    {
        QUuid id;
        QUuid runtimeId;
        QString customization;

        bool safeMode;
        VersionHolder version;
        QString os;
        QString displayAddress;
        Constants::ServerFlags flags;
        
        QString name;
        QString systemName;
        QString hostAddress;
        int port;

        bool accessibleByHttp;

        BaseServerInfo();

    };
    
    bool operator == (const BaseServerInfo &first
        , const BaseServerInfo &second);
    
    bool operator != (const BaseServerInfo &first
        , const BaseServerInfo &second);


    struct ExtraServerInfo
    {
        QString password;
        qint64 timestampMs;
        qint64 utcDateTimeMs;
        QByteArray timeZoneId;
        InterfaceInfoList interfaces;

        Constants::SystemCommands sysCommands;

        typedef QHash<Constants::SystemCommands, QString> ScriptNames;
        ScriptNames scriptNames;

        ExtraServerInfo();
        
        ExtraServerInfo(const QString &initPassword
            , const qint64 &initTimestampMs
            , const qint64 &initUtcDateTimeMs
            , const QByteArray &initTimeZoneId
            , const InterfaceInfoList initInterfaces
            , const Constants::SystemCommands initSysCommands
            , const ScriptNames &initScriptNames);
    };
    
    class ServerInfo
    {
    public:
        ServerInfo();
        
        ServerInfo(const ServerInfo &other);
        
        ServerInfo(const BaseServerInfo &baseInfo);
        
        ServerInfo(const BaseServerInfo &baseInfo
            , const ExtraServerInfo &extraInfo);

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

