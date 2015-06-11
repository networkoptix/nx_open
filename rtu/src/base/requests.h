
#pragma once

#include <functional>

#include <QUuid>

#include <base/types.h>
#include <base/server_info.h>

class QDateTime;
class QTimeZone;
class QString;

namespace rtu
{
    class HttpClient;
    
    const QString &adminUserName();
    const QStringList &defaultAdminPasswords();
    
    typedef std::function<void (const QUuid &serverId)> FailedCallback;
    
    enum AffectedEntity
    {
        kNoEntitiesAffected = 0x0
        , kPortAffected             = 0x1
        , kPasswordAffected         = 0x2
        , kSystemNameAffected       = 0x4

        , kIpAddressAffected        = 0x8
        , kMaskAffected       = 0x10
        , kDHCPUsageAffected        = 0x20
        , kAllAddressFlagsAffected  = kIpAddressAffected | kMaskAffected | kDHCPUsageAffected

        , kDateTimeAffected         = 0x40
        , kTimeZoneAffected         = 0x80
        
        , kAllEntitiesAffected      = 0xFF
    };
    
    Q_DECLARE_FLAGS(AffectedEntities, AffectedEntity)
    Q_DECLARE_OPERATORS_FOR_FLAGS(AffectedEntities)
    
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
        , const FailedCallback &failed);
    
    bool getTimeRequest(HttpClient *client
        , const ServerInfo &info
        , const DateTimeCallbackType &successfullCallback
        , const FailedCallback &failedCallback);
    
    void interfacesListRequest(HttpClient *client
        , const BaseServerInfo &info
        , const QString &password
        , const ExtraServerInfoSuccessCallback &successful
        , const FailedCallback &failed);

    ///

    bool sendSetTimeRequest(HttpClient *client
        , const ServerInfo &info
        , const QDateTime &utcDateTime
        , const QTimeZone &timeZone
        , const OperationCallback &callback);

    ///

    bool sendSetSystemNameRequest(HttpClient *client
        , const ServerInfo &info
        , const QString &systemName
        , const OperationCallback &callback);
    
    bool sendSetPasswordRequest(HttpClient *client
        , const ServerInfo &info
        , const QString &password
        , bool useNewPassword
        , const OperationCallback &callback);

    bool sendSetPortRequest(HttpClient *client
        , const ServerInfo &info
        , int port
        , const OperationCallback &callback);

    
    ///

    struct ItfUpdateInfo
    {
        QString name;

        bool useDHCP;
        StringPointer ip;
        StringPointer mask;

        ItfUpdateInfo();

        ItfUpdateInfo(const ItfUpdateInfo &other);
        
        ItfUpdateInfo(const QString &initName
            , bool initUseDHCP
            , const StringPointer &initIp = StringPointer()
            , const StringPointer &initMask = StringPointer());
        
        ItfUpdateInfo &operator =(const ItfUpdateInfo &other);
    };

    typedef QVector<ItfUpdateInfo> ItfUpdateInfoContainer;

    void sendChangeItfRequest(HttpClient *client
        , const ServerInfo &infos
        , const ItfUpdateInfoContainer &updateInfo
        , const OperationCallback &callback);
}
