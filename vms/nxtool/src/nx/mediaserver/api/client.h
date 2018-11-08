
#pragma once

#include <memory>
#include <functional>
#include <QObject>
#include <QUuid>

#include <nx/rest/rest_client.h>
#include <nx/mediaserver/api/server_info.h>

class QDateTime;
class QTimeZone;
class QString;

namespace nx {
namespace mediaserver {
namespace api {

/** singleton
 * Executes requests to Mediaserver via Rest API.
 */
class Client
    : public QObject
{
    Q_OBJECT
public:
    static Client& instance();

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

    enum class ResultCode
    {
        kSuccess
        , kRequestTimeout
        , kUnauthorized
        , kInternalAppError
        , kInvalidParam
        , kUnspecified
    };

    static bool parseModuleInformationReply(const QJsonObject &reply
        , BaseServerInfo *baseInfo);

    static const QStringList &defaultAdminPasswords();

    static const qint64 kDefaultTimeoutMs = 10 * 1000;

    struct ItfUpdateInfo
    {
        QString name;

        std::shared_ptr<bool> useDHCP;
        std::shared_ptr<QString> ip;
        std::shared_ptr<QString> mask;
        std::shared_ptr<QString> dns;
        std::shared_ptr<QString> gateway;

        ItfUpdateInfo();

        ItfUpdateInfo(const ItfUpdateInfo &other);

        ItfUpdateInfo(const QString &initName);

        ItfUpdateInfo &operator =(const ItfUpdateInfo &other);
    };

    typedef QVector<ItfUpdateInfo> ItfUpdateInfoContainer;
    typedef std::shared_ptr<ItfUpdateInfoContainer> ItfUpdateInfoContainerPtr;

    typedef std::function<void (const ResultCode errorCode
        , Client::AffectedEntities affectedEntities)> OperationCallback;

    typedef std::function<void (const ResultCode errorCode
        , Client::AffectedEntities affectedEntities
        , bool needRestart)> OperationCallbackEx;

    typedef std::function<void (const QUuid &serverId
        , const QDateTime &utcServerTime
        , const QTimeZone &timeZone
        , const QDateTime &timestamp)> DateTimeCallbackType;

    typedef std::function<void (BaseServerInfo *info)> BaseServerInfoCallback;

    typedef std::function<void (const QUuid &id
        , const ExtraServerInfo &extraInfo)> ExtraServerInfoSuccessCallback;

    static void multicastModuleInformation(const QUuid &id
        , const BaseServerInfoCallback &successCallback
        , const OperationCallback &failedCallback
        , qint64 timeoutMs);

    static void getTime(const BaseServerInfoPtr &baseInfo
        , const QString &password
        , const ExtraServerInfoSuccessCallback &successful
        , const OperationCallback &failed
        , qint64 timeoutMs);

    static void getIfList(const BaseServerInfoPtr &baseInfo
        , const QString &password
        , const ExtraServerInfoSuccessCallback &successful
        , const OperationCallback &failed
        , qint64 timeoutMs);

    /**
     * Check authorization and send getTime and getIfList consequentially.
     * If authorized and getTime is successful, call successful callback anyway.
     */
    static void getServerExtraInfo(const BaseServerInfoPtr &baseInfo
        , const QString &password
        , const ExtraServerInfoSuccessCallback &successful
        , const OperationCallback &failed
        , qint64 timeoutMs);

    static void sendIfListRequest(const BaseServerInfoPtr &info
        , const QString &password
        , const ExtraServerInfoSuccessCallback &successful
        , const OperationCallback &failed
        , qint64 timeoutMs);

    static void sendRestartRequest(const BaseServerInfoPtr &info
        , const QString &password
        , const OperationCallback &callback);

    static void sendSystemCommand(const BaseServerInfoPtr &info
        , const QString &password
        , const Constants::SystemCommand sysCommand
        , const OperationCallback &callback);

    ///

    static void sendSetTimeRequest(const BaseServerInfoPtr &baseInfo
        , const QString &password
        , qint64 utcDateTimeMs
        , const QByteArray &timeZoneId
        , const OperationCallbackEx &callback);

    static void sendSetSystemNameRequest(const BaseServerInfoPtr &baseInfo
        , const QString &password
        , const QString &systemName
        , const OperationCallbackEx &callback);

    static void sendSetPasswordAndLocalIdRequest(const BaseServerInfoPtr &baseInfo
        , const QString &currentPassword
        , const QString &password
        , const QUuid &localSystemId
        , bool useNewPassword
        , const OperationCallbackEx &callback);

    static void sendSetPortRequest(const BaseServerInfoPtr &baseInfo
        , const QString &password
        , int port
        , const OperationCallbackEx &callback);

    static void sendChangeItfRequest(const BaseServerInfoPtr &baseInfo
        , const QString &password
        , const ItfUpdateInfoContainer &updateInfo
        , const OperationCallbackEx &callback);

signals:
    /**
     * Emits when command has failed by http, but successfully executed by multicast.
     * Can be also emitted if the access method remains unchanged.
     */
    void accessMethodChanged(const QUuid& id, HttpAccessMethod newHttpAccessMethod);

private:
    typedef std::function<bool (const QJsonObject &object
        , ExtraServerInfo &extraInfo)> ParseFunction;

    typedef rest::RestClient RestClient;

    static void acceptNewHttpAccessMethod(
        RestClient::NewHttpAccessMethod newHttpAccessMethod,
        const BaseServerInfoPtr& baseInfo);

    static void getScriptInfos(
        const BaseServerInfoPtr &baseInfo,
        const QString &password,
        const Client::ExtraServerInfoSuccessCallback &successCallback,
        const Client::OperationCallback &failedCallback,
        qint64 timeoutMs);

    static RestClient::SuccessCallback makeSuccessCallbackEx(
        const Client::ExtraServerInfoSuccessCallback& successful,
        const Client::OperationCallback& failed,
        const QString& password,
        Client::AffectedEntities affected,
        const ParseFunction& parser,
        const BaseServerInfoPtr& baseInfo);

    static RestClient::SuccessCallback makeSuccessCallbackEx(
        const Client::OperationCallbackEx& callback,
        Client::AffectedEntities affected,
        const BaseServerInfoPtr& baseInfo);

    static RestClient::SuccessCallback makeSuccessCallback(
        const Client::OperationCallback& callback,
        Client::AffectedEntities affected,
        const BaseServerInfoPtr& baseInfo);

    static RestClient::ErrorCallback makeErrorCallbackEx(
        const Client::OperationCallbackEx& callback,
        Client::AffectedEntities affected,
        const BaseServerInfoPtr& baseInfo);

    static RestClient::ErrorCallback makeErrorCallback(
        const Client::OperationCallback& callback,
        Client::AffectedEntities affected,
        const BaseServerInfoPtr& baseInfo);

    static void checkAuth(const BaseServerInfoPtr& baseInfo,
        const QString& password,
        const Client::OperationCallback& callback,
        qint64 timeoutMs);

    Client();
    virtual ~Client();

    std::unique_ptr<RestClient> m_restClient; 
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Client::AffectedEntities)

} // namespace api
} // namespace mediaserver
} // namespace nx
