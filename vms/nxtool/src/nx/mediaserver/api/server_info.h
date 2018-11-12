
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

#include <nx/mediaserver/api/version_holder.h>

namespace nx {
namespace mediaserver {
namespace api {

struct InterfaceInfo;
struct BaseServerInfo;
struct ExtraServerInfo;
class ServerInfo;

typedef QList<InterfaceInfo> InterfaceInfoList;
typedef QVector<ServerInfo *> ServerInfoPtrContainer;
typedef QVector<ServerInfo> ServerInfoContainer;
typedef std::shared_ptr<ServerInfo> ServerInfoPtr;
typedef std::shared_ptr<BaseServerInfo> BaseServerInfoPtr;

// TOOD mike: RENAME Constants; qml
class Constants: public QObject {
    Q_OBJECT

public:

    enum ServerFlag
    {
        NoFlags                             = 0x00
        , AllowChangeDateTimeFlag           = 0x01
        , AllowIfConfigFlag                 = 0x02
        , IsFactoryFlag                     = 0x04 //< Old systems (version < 3.0) factory tag
        , IsNewSystemFlag                   = 0x08 | IsFactoryFlag
        , HasHdd                            = 0x10

        , AllFlags                          = 0xFF
    };
    Q_ENUMS(ServerFlag)
    Q_DECLARE_FLAGS(ServerFlags, ServerFlag)

    enum SystemCommand
    {
        NoCommands                          = 0x00

        , RestartServerCmd                  = 0x01
        , RebootCmd                         = 0x02
        , FactoryDefaultsCmd                = 0x04
        , FactoryDefaultsButNetworkCmd      = 0x08

        , AllCommands                       = 0xFF
    };
    Q_ENUMS(SystemCommand)
    Q_DECLARE_FLAGS(SystemCommands, SystemCommand)
};

enum class HttpAccessMethod
{
    kTcp,
    kUdp,
};

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

    HttpAccessMethod httpAccessMethod;

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

    typedef QScopedPointer<ExtraServerInfo> ExtraServerInfoPtr;

    BaseServerInfo m_base;
    ExtraServerInfoPtr m_extra;
};

} // namespace api
} // namespace mediaserver
} // namespace nx

Q_DECLARE_METATYPE(nx::mediaserver::api::InterfaceInfo)
Q_DECLARE_METATYPE(nx::mediaserver::api::HttpAccessMethod)
