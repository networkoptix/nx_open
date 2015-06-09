
#pragma once

#include <functional>

#include <QUuid>

#include <server_info.h>

class QDateTime;
class QTimeZone;
class QString;

namespace rtu
{
    class HttpClient;
    
    const QString &adminUserName();
    const QString &defaultAdminPassword();
    
    typedef std::function<void (const QUuid &serverId)> FailedCallback;
    
    enum AffectedEntity
    {
        kPort           = 0x1
        , kPassword     = 0x2
        , kSystemName   = 0x4
        , kIpAddress    = 0x8
        , kSubnetMask   = 0x10
        , kDHCPUsage    = 0x20
    };
    
    Q_DECLARE_FLAGS(AffectedEntities, AffectedEntity)
    Q_DECLARE_OPERATORS_FOR_FLAGS(AffectedEntities)
    
    typedef std::function<void (const QString &errorReason
        , AffectedEntities affectedEntities)> OperationCallback; 
    
    typedef std::function<void (const QUuid &serverId
        , const QDateTime &serverTime
        , const QDateTime &responseArrivalUtcTime)> DateTimeCallbackType;
    
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
    
    bool setTimeRequest(HttpClient *client
        , const ServerInfo &info
        , const QDateTime &dateTime
        , const DateTimeCallbackType &succesfulCallback
        , const FailedCallback &failedCallback);
    ///

    bool changeIpsRequest(HttpClient *client
        , const ServerInfo &info
        , const InterfaceInfoList &addresses
        , const OperationCallback &callback);
}
