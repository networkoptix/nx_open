
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
        , const OperationCallback &failedCallback
        , int timeout);

    void getTime(const BaseServerInfoPtr &baseInfo
        , const QString &password
        , const ExtraServerInfoSuccessCallback &successful
        , const OperationCallback &failed
        , int timeout = RestClient::kUseStandardTimeout);

    void getIfList(const BaseServerInfoPtr &baseInfo
        , const QString &password
        , const ExtraServerInfoSuccessCallback &successful
        , const OperationCallback &failed
        , int timeout = RestClient::kUseStandardTimeout);

    /// Checks authorization and sends getTime and getIfList consequentially. 
    /// If authorized and getTime is successful, calls successfull callback anyway
    void getServerExtraInfo(const BaseServerInfoPtr &baseInfo
        , const QString &password
        , const ExtraServerInfoSuccessCallback &successful
        , const OperationCallback &failed
        , int timeout = RestClient::kUseStandardTimeout);

    void sendIfListRequest(const BaseServerInfoPtr &info
        , const QString &password
        , const ExtraServerInfoSuccessCallback &successful
        , const OperationCallback &failed
        , int timeout = RestClient::kUseStandardTimeout);

    void sendRestartRequest(const BaseServerInfoPtr &info
        , const QString &password
        , const OperationCallback &callback);

    void sendSystemCommand(const BaseServerInfoPtr &info
        , const QString &password
        , const Constants::SystemCommand sysCommand
        , const OperationCallback &callback);

    ///

    void sendSetTimeRequest(const BaseServerInfoPtr &baseInfo
        , const QString &password
        , qint64 utcDateTimeMs
        , const QByteArray &timeZoneId
        , const OperationCallback &callback);

    void sendSetSystemNameRequest(const BaseServerInfoPtr &baseInfo
        , const QString &password
        , const QString &systemName
        , const OperationCallback &callback);
    
    void sendSetPasswordRequest(const BaseServerInfoPtr &baseInfo
        , const QString &currentPassword
        , const QString &password
        , bool useNewPassword
        , const OperationCallback &callback);

    void sendSetPortRequest(const BaseServerInfoPtr &baseInfo
        , const QString &password
        , int port
        , const OperationCallback &callback);

    void sendChangeItfRequest(const rtu::BaseServerInfoPtr &baseInfo
        , const QString &password
        , const ItfUpdateInfoContainer &updateInfo
        , const OperationCallback &callback);
}
