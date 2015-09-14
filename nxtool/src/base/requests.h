
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
    const QString &adminUserName();
    const QStringList &defaultAdminPasswords();
    
    bool parseModuleInformationReply(const QJsonObject &reply
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
    
    typedef std::function<void (const RequestError errorCode
        , Constants::AffectedEntities affectedEntities)> OperationCallback; 
    
    typedef std::function<void (const QUuid &serverId
        , const QDateTime &utcServerTime
        , const QTimeZone &timeZone
        , const QDateTime &timestamp)> DateTimeCallbackType;
    
    typedef std::function<void (BaseServerInfo &info)> BaseServerInfoCallback;

    typedef std::function<void (const QUuid &id
        , const ExtraServerInfo &extraInfo)> ExtraServerInfoSuccessCallback;
    
    void multicastModuleInformation(const QUuid &id
        , const BaseServerInfoCallback &successCallback
        , const OperationCallback &failedCallback);

    void getTime(const BaseServerInfo &baseInfo
        , const QString &password
        , const ExtraServerInfoSuccessCallback &successful
        , const OperationCallback &failed
        , int timeout = HttpClient::kUseDefaultTimeout);

    void getIfList(const BaseServerInfo &baseInfo
        , const QString &password
        , const ExtraServerInfoSuccessCallback &successful
        , const OperationCallback &failed
        , int timeout = HttpClient::kUseDefaultTimeout);

    /// Sends getTime and getIfList consequentially. 
    /// If getTime is successful, calls successfull callback anyway
    void getServerExtraInfo(const BaseServerInfo &baseInfo
        , const QString &password
        , const ExtraServerInfoSuccessCallback &successful
        , const OperationCallback &failed
        , int timeout = HttpClient::kUseDefaultTimeout);

    void sendIfListRequest(const BaseServerInfo &info
        , const QString &password
        , const ExtraServerInfoSuccessCallback &successful
        , const OperationCallback &failed
        , int timeout = HttpClient::kUseDefaultTimeout);

    ///

    void sendSetTimeRequest(const ServerInfo &info
        , qint64 utcDateTimeMs
        , const QByteArray &timeZoneId
        , const OperationCallback &callback);

    void sendSetSystemNameRequest(const ServerInfo &info
        , const QString &systemName
        , const OperationCallback &callback);
    
    void sendSetPasswordRequest(const ServerInfo &info
        , const QString &password
        , bool useNewPassword
        , const OperationCallback &callback);

    void sendSetPortRequest(const ServerInfo &info
        , int port
        , const OperationCallback &callback);

    void sendChangeItfRequest(const ServerInfo &infos
        , const ItfUpdateInfoContainer &updateInfo
        , const OperationCallback &callback);
}
