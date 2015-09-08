
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
    typedef std::shared_ptr<ItfUpdateInfoContainer> ItfUpdateInfoContainerPointer;
    
    typedef std::function<void (const int errorCode
        , const QString &errorReason
        , Constants::AffectedEntities affectedEntities)> OperationCallback; 
    
    typedef std::function<void (const QUuid &serverId
        , const QDateTime &utcServerTime
        , const QTimeZone &timeZone
        , const QDateTime &timestamp)> DateTimeCallbackType;
    
    enum 
    {
        kNoErrorReponse = 0 
        , kUnspecifiedError = -1

        , kUnauthorizedError = 401
    };
    
    typedef std::function<void (const QUuid &id
        , const rtu::ExtraServerInfo &extraInfo)> ExtraServerInfoSuccessCallback;
    
    void getTime(HttpClient *client
        , const BaseServerInfo &baseInfo
        , const QString &password
        , const ExtraServerInfoSuccessCallback &successful
        , const OperationCallback &failed
        , int timeout = HttpClient::kUseDefaultTimeout);

    void getIfList(HttpClient *client
        , const BaseServerInfo &baseInfo
        , const QString &password
        , const ExtraServerInfoSuccessCallback &successful
        , const OperationCallback &failed
        , int timeout = HttpClient::kUseDefaultTimeout);

    /// Sends getTime and getIfList consequentially. 
    /// If getTime is successful, calls successfull callback anyway
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
