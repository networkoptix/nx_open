
#pragma once

#include <functional>

#include <QUuid>

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
        , kPort             = 0x1
        , kPassword         = 0x2
        , kSystemName       = 0x4
        , kIpAddress        = 0x8
        , kSubnetMask       = 0x10
        , kDHCPUsage        = 0x20
        , kDateTime         = 0x40
        , kTimeZone         = 0x80
        , kAllFlags         = 0xFF
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
    
    bool configureRequest(HttpClient *client
        , const OperationCallback &callback
        , const ServerInfo &info
        , const QString &systemName
        , const QString &password
        , const int newPort);
    
    bool getTimeRequest(HttpClient *client
        , const ServerInfo &info
        , const DateTimeCallbackType &successfullCallback
        , const FailedCallback &failedCallback);
    
    void interfacesListRequest(HttpClient *client
        , const BaseServerInfo &info
        , const QString &password
        , const ExtraServerInfoSuccessCallback &successful
        , const FailedCallback &failed);

    void setTimeRequest(HttpClient *client
        , const ServerInfo &info
        , const QDateTime &utcDateTime
        , const QTimeZone &timeZone
        , const DateTimeCallbackType &successfulCallback
        , const OperationCallback &failedCallback);
    ///

    void changeIpsRequest(HttpClient *client
        , const ServerInfo &info
        , const InterfaceInfoList &addresses
        , const OperationCallback &callback);
}
