
#pragma once

#include <functional>

#include <QUuid>

#include <base/types.h>
#include <base/server_info.h>
#include <helpers/http_client.h>

class QDateTime;
class QTimeZone;
class QString;

namespace rtu
{
    class HttpClient;
    
    const QString &adminUserName();
    const QStringList &defaultAdminPasswords();
    
    void parseModuleInformationReply(const QJsonObject &reply
        , rtu::BaseServerInfo &baseInfo);

    /// TODO: #ynikitenkov think about usage and rename if necessary to "invalid entity"
    enum AffectedEntity
    {
        kNoEntitiesAffected         = 0x0
        , kPortAffected             = 0x1
        , kPasswordAffected         = 0x2
        , kSystemNameAffected       = 0x4

        , kIpAddressAffected        = 0x10
        , kMaskAffected             = 0x20
        , kDHCPUsageAffected        = 0x30
        , kDNSAffected              = 0x40
        , kGatewayAffected          = 0x80
        , kAllAddressFlagsAffected  = kIpAddressAffected | kMaskAffected | kDHCPUsageAffected
            | kDNSAffected | kGatewayAffected

        , kDateTimeAffected         = 0x100
        , kTimeZoneAffected         = 0x200
        
        , kAllEntitiesAffected      = 0xFFF
    };
    
    Q_DECLARE_FLAGS(AffectedEntities, AffectedEntity)
    Q_DECLARE_OPERATORS_FOR_FLAGS(AffectedEntities)

    ///

    struct ItfUpdateInfo
    {
        QString name;

        BoolPointer useDHCP;
        StringPointer ip;
        StringPointer mask;
        StringPointer dns;
        StringPointer gateway;

        ItfUpdateInfo();

        ItfUpdateInfo(const ItfUpdateInfo &other);
        
        ItfUpdateInfo(const QString &initName);
        
        ItfUpdateInfo &operator =(const ItfUpdateInfo &other);
    };

    typedef QVector<ItfUpdateInfo> ItfUpdateInfoContainer;
    
    typedef std::function<void (const QString &errorReason
        , AffectedEntities affectedEntities)> OperationCallback; 
    
    typedef std::function<void (const QUuid &serverId
        , const QDateTime &utcServerTime
        , const QTimeZone &timeZone
        , const QDateTime &timestamp)> DateTimeCallbackType;
    
    enum { kNoErrorReponse = 0 };
    
    typedef std::function<void (const QUuid &id
        , const rtu::ExtraServerInfo &extraInfo)> ExtraServerInfoSuccessCallback;
    
    void getServerExtraInfo(HttpClient *client
        , const BaseServerInfo &baseInfo
        , const QString &password
        , const ExtraServerInfoSuccessCallback &successful
        , const OperationCallback &failed
        , int timeout = HttpClient::kUseDefaultTimeout);

    void sendIfListRequest(HttpClient *client
        , const BaseServerInfo &info
        , const QString &password
        , const ExtraServerInfoSuccessCallback &successful
        , const OperationCallback &failed
        , int timeout = HttpClient::kUseDefaultTimeout);

    ///

    void sendSetTimeRequest(HttpClient *client
        , const ServerInfo &info
        , qint64 utcDateTimeMs
        , const QByteArray &timeZoneId
        , const OperationCallback &callback);

    ///

    void sendSetSystemNameRequest(HttpClient *client
        , const ServerInfo &info
        , const QString &systemName
        , const OperationCallback &callback);
    
    void sendSetPasswordRequest(HttpClient *client
        , const ServerInfo &info
        , const QString &password
        , bool useNewPassword
        , const OperationCallback &callback);

    void sendSetPortRequest(HttpClient *client
        , const ServerInfo &info
        , int port
        , const OperationCallback &callback);

    void sendChangeItfRequest(HttpClient *client
        , const ServerInfo &infos
        , const ItfUpdateInfoContainer &updateInfo
        , const OperationCallback &callback);
}
