
#pragma once

#include <functional>

#include <QUuid>

#include <base/types.h>
#include <base/server_info.h>

class QDateTime;
class QTimeZone;
class QString;

// TODO mike: RENAME
namespace rtu
{
    // TODO mike: Convert to QObject.

    // TODO mike: Rename to RequestResult.
    enum class RequestError
    {
        kSuccess
        , kRequestTimeout
        , kUnauthorized
        , kInternalAppError
        , kInvalidParam
        , kUnspecified
    };

    bool parseModuleInformationReply(const QJsonObject &reply
        , BaseServerInfo *baseInfo);

    const QStringList &defaultAdminPasswords();

    const qint64 kDefaultTimeoutMs = 10 * 1000;

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

    typedef std::function<void (const RequestError errorCode
        , Constants::AffectedEntities affectedEntities
        , bool needRestart)> OperationCallbackEx; 
    
    typedef std::function<void (const QUuid &serverId
        , const QDateTime &utcServerTime
        , const QTimeZone &timeZone
        , const QDateTime &timestamp)> DateTimeCallbackType;
    
    typedef std::function<void (BaseServerInfo *info)> BaseServerInfoCallback;

    typedef std::function<void (const QUuid &id
        , const ExtraServerInfo &extraInfo)> ExtraServerInfoSuccessCallback;
    
    void multicastModuleInformation(const QUuid &id
        , const BaseServerInfoCallback &successCallback
        , const OperationCallback &failedCallback
        , qint64 timeoutMs);

    void getTime(const BaseServerInfoPtr &baseInfo
        , const QString &password
        , const ExtraServerInfoSuccessCallback &successful
        , const OperationCallback &failed
        , qint64 timeoutMs);

    void getIfList(const BaseServerInfoPtr &baseInfo
        , const QString &password
        , const ExtraServerInfoSuccessCallback &successful
        , const OperationCallback &failed
        , qint64 timeoutMs);

    /// Checks authorization and sends getTime and getIfList consequentially. 
    /// If authorized and getTime is successful, calls successful callback anyway.
    void getServerExtraInfo(const BaseServerInfoPtr &baseInfo
        , const QString &password
        , const ExtraServerInfoSuccessCallback &successful
        , const OperationCallback &failed
        , qint64 timeoutMs);

    void sendIfListRequest(const BaseServerInfoPtr &info
        , const QString &password
        , const ExtraServerInfoSuccessCallback &successful
        , const OperationCallback &failed
        , qint64 timeoutMs);

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
        , const OperationCallbackEx &callback);

    void sendSetSystemNameRequest(const BaseServerInfoPtr &baseInfo
        , const QString &password
        , const QString &systemName
        , const OperationCallbackEx &callback);
    
    void sendSetPasswordRequest(const BaseServerInfoPtr &baseInfo
        , const QString &currentPassword
        , const QString &password
        , bool useNewPassword
        , const OperationCallbackEx &callback);

    void sendSetPortRequest(const BaseServerInfoPtr &baseInfo
        , const QString &password
        , int port
        , const OperationCallbackEx &callback);

    void sendChangeItfRequest(const rtu::BaseServerInfoPtr &baseInfo
        , const QString &password
        , const ItfUpdateInfoContainer &updateInfo
        , const OperationCallbackEx &callback);
}
