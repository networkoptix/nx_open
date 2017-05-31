
#include "client.h"

#include <cassert>
#include <memory>
#include <QUrl>
#include <QUrlQuery>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

#include <nx/mediaserver/api/version_holder.h>
#include <base/nxtool_app_info.h>

namespace nx {
namespace mediaserver {
namespace api {

using rest::RestClient;

namespace {

/// Extra abilities = SF_Has_Hdd flag and ability to execute script through the /api/execute rest command
const VersionHolder kExtraAbilitiesMinVersion = VersionHolder(QStringLiteral("2.4.1"));

const QString kApiNamespaceTag = "/api/";

const QString kModuleInfoAuthCommand = kApiNamespaceTag + "moduleInformationAuthenticated";
const QString kConfigureCommand = kApiNamespaceTag + "configure";
const QString kIfConfigCommand = kApiNamespaceTag + "ifconfig";
const QString kSetTimeCommand = kApiNamespaceTag + "settime";
const QString kGetTimeCommand = kApiNamespaceTag + "gettime";
const QString kIfListCommand = kApiNamespaceTag + "iflist";
const QString kRestartCommand = kApiNamespaceTag + "restart";
const QString kScriptsList = kApiNamespaceTag + "scriptList";
const QString kExecuteCommand = kApiNamespaceTag + "execute";

const QString kIDTag = "id";
const QString kSeedTag = "seed";
const QString kRuntimeIdTag = "runtimeId";
const QString kVersionTag = "version";
const QString kNameTag = "name";
const QString kSystemNameTag = "systemName";
const QString kPortTag = "port";
const QString kFlagsTag = "flags";
const QString kServerFlagsTag = "serverFlags";
const QString kLocalTimeFlagTag = "local";
const QString kSafeModeTag = "ecDbReadOnly";
const QString kSysInfoTag = "systemInformation";
const QString kCustomizationTag = "customization";

const QString kUserAgent = QString("%1/%2")
    .arg(rtu::ApplicationInfo::applicationDisplayName(),
         rtu::ApplicationInfo::applicationVersion());

const QString kAdminUserName = QStringLiteral("admin");

static QUrl makeUrl(const QString &host
    , const int port
    , const QString &password
    , const QString &command)
{
    QUrl url;
    url.setScheme("http");
    url.setHost(host);
    url.setPort(port);
    url.setUserName(kAdminUserName);
    url.setPassword(password);
    url.setPath(command);
    return url;
}

static QUrl makeUrl(const ServerInfo &info
    , const QString &command)
{
    if (!info.hasExtraInfo())
        return QUrl();

    return makeUrl(info.baseInfo().hostAddress, info.baseInfo().port
        , info.extraInfo().password, command);
}

static QString convertCapsToReadable(const QString &customization
    , const QString &caps)
{
    static const auto kWindowsTag = QStringLiteral("windows");
    static const auto kLinuxTag = QStringLiteral("linux");
    static const auto kX64Tag = QStringLiteral("x64");
    static const auto kX86Tag = QStringLiteral("x86");
    static const auto kArmTag = QStringLiteral("arm");

    const bool isX86 = caps.contains(kX86Tag);
    const bool isX64 = caps.contains(kX64Tag);
    const bool isArm = caps.contains(kArmTag);

    const QString arch = (isX86 ? kX86Tag : (isX64 ? kX64Tag : QString()));
    if (caps.contains(kWindowsTag))
    {
        return QStringLiteral("%1 %2").arg(QStringLiteral("Windows")).arg(arch);
    }
    else if (caps.contains(kLinuxTag))
    {
        if (!isArm)
            return QStringLiteral("%1 %2").arg(QStringLiteral("Linux")).arg(arch);
        else
        {
            if (caps.contains("bpi"))
            {
                static const auto vista = QStringLiteral("vista");
                return (customization.contains(vista) ?
                    QStringLiteral("Q") : QStringLiteral("Nx1"));
            }
            else if (caps.contains("rpi"))
                return QStringLiteral("Raspberry Pi");
            else if (caps.contains("isd"))
                return QStringLiteral("ISD Jaguar");
            else if (caps.contains("isd_s2"))
                return QStringLiteral("ISD S2");
            else if (caps.contains("edge1"))
                return QStringLiteral("Edge 1");
            else
                return QStringLiteral("%1 %2").arg(kLinuxTag).arg("unknown");
        }
    }

    return QStringLiteral("Unknown OS");
}

static Client::ResultCode convertRestClientRequestResult(
    RestClient::ResultCode restResult)
{
    switch (restResult)
    {
        case RestClient::ResultCode::kSuccess: return Client::ResultCode::kSuccess;
        case RestClient::ResultCode::kRequestTimeout: return Client::ResultCode::kRequestTimeout;
        case RestClient::ResultCode::kUnauthorized: return Client::ResultCode::kUnauthorized;
        case RestClient::ResultCode::kUnspecified: return Client::ResultCode::kUnspecified;
        default:
            Q_ASSERT(false);
            return Client::ResultCode::kUnspecified;
    }
}

typedef QHash<QString, std::function<void (const QJsonObject &object
    , BaseServerInfo *info)> > KeyParserContainer;
const KeyParserContainer parser = []() -> KeyParserContainer
{
    KeyParserContainer result;
    result.insert(kIDTag, [](const QJsonObject& object, BaseServerInfo *info)
        { info->id = QUuid(object.value(kIDTag).toString()); });
    result.insert(kSeedTag, [](const QJsonObject& object, BaseServerInfo *info)
        { info->id = QUuid(object.value(kSeedTag).toString()); });
    result.insert(kRuntimeIdTag, [](const QJsonObject& object, BaseServerInfo *info)
        { info->runtimeId = QUuid(object.value(kRuntimeIdTag).toString()); });
    result.insert(kNameTag, [](const QJsonObject& object, BaseServerInfo *info)
        { info->name = object.value(kNameTag).toString(); });
    result.insert(kSystemNameTag, [](const QJsonObject& object, BaseServerInfo *info)
        { info->systemName = object.value(kSystemNameTag).toString(); });
    result.insert(kPortTag, [](const QJsonObject& object, BaseServerInfo *info)
        { info->port = object.value(kPortTag).toInt(); });
    result.insert(kVersionTag, [](const QJsonObject& object, BaseServerInfo *info)
        { info->version = VersionHolder(object.value(kVersionTag).toString()); });
    result.insert(kSafeModeTag, [](const QJsonObject& object, BaseServerInfo *info)
        { info->safeMode = object.value(kSafeModeTag).toBool(); });
    result.insert(kCustomizationTag, [](const QJsonObject& object, BaseServerInfo *info)
        { info->customization = object.value(kCustomizationTag).toString(); });

    result.insert(kSysInfoTag, [](const QJsonObject& object, BaseServerInfo *info)
    {
        const auto val = object.value(kSysInfoTag);
        const auto sysInfo = val.toObject();

        /// We use info.os to temporary store OS caps.
        if (sysInfo.isEmpty())
        {
            info->os = val.toString();
        }
        else
        {
            static const auto kPlatformKey = QStringLiteral("platform");
            static const auto kModKey = QStringLiteral("modification");
            static const auto kArchKey = QStringLiteral("arch");

            const QString platform = (sysInfo.keys().contains(kPlatformKey)
                ? sysInfo.value(kPlatformKey).toString() : QString());
            const QString modification = (sysInfo.keys().contains(kModKey)
                ? sysInfo.value(kModKey).toString() : QString());
            const QString arch = (sysInfo.keys().contains(kArchKey)
                ? sysInfo.value(kArchKey).toString() : QString());

            info->os = QStringLiteral("%1 %2 %3").arg(platform, arch, modification);
        }
    });

    const auto parseFlags = [](const QJsonObject& object, BaseServerInfo *info, const QString &tagName)
    {
            typedef QPair<QString, Constants::ServerFlag> TextFlagsInfo;
            static const TextFlagsInfo kKnownFlags[] =
            {
                TextFlagsInfo("SF_timeCtrl", Constants::ServerFlag::AllowChangeDateTimeFlag)
                , TextFlagsInfo("SF_IfListCtrl", Constants::ServerFlag::AllowIfConfigFlag)
                , TextFlagsInfo("SF_AutoSystemName", Constants::ServerFlag::IsFactoryFlag)
                , TextFlagsInfo("SF_Has_HDD", Constants::ServerFlag::HasHdd)
            };

            info->flags = Constants::ServerFlag::NoFlags;
            const QString textFlags = object.value(tagName).toString();
            for (const TextFlagsInfo &flagInfo: kKnownFlags)
            {
                if (textFlags.contains(flagInfo.first))
                    info->flags |= flagInfo.second;
            }
    };

    result.insert(kServerFlagsTag, [parseFlags](const QJsonObject& object, BaseServerInfo *info)
        { parseFlags(object, info, kServerFlagsTag); });

    /// Supports old versions of server where serverFlags field was "flags"
    result.insert(kFlagsTag, [parseFlags](const QJsonObject& object, BaseServerInfo *info)
        { parseFlags(object, info, kFlagsTag); });
    return result;
}();

/// Parsers stuff

static bool isErrorReply(const QJsonObject &object)
{
    static const QString &kErrorTag = "error";
    if (!object.contains(kErrorTag))
        return false;

    /// in response int is string! - for example: error: "3"
    const int code = object.value(kErrorTag).toString().toInt();
    return (code != 0);
}

///

static QJsonValue extractReplyBody(const QJsonObject &object)
{
    static const QString &kReplyTag = "reply";
    return (object.contains(kReplyTag) ? object.value(kReplyTag) : QJsonValue());
}

static bool needRestart(const QJsonObject &object)
{
    static const auto kNeedRestartTag = "rebootNeeded";

    const auto body = extractReplyBody(object).toObject();
    return (body.contains(kNeedRestartTag)
        ? body.value(kNeedRestartTag).toBool() : false);
}

///

static bool parseIfListCmd(const QJsonObject &object
    , ExtraServerInfo &extraInfo)
{
    if (isErrorReply(object))
        return false;

    const QJsonArray &ifArray = extractReplyBody(object).toArray();
    if (ifArray.isEmpty())
        return false;

    InterfaceInfoList interfaces;
    for(const QJsonValue &interface: ifArray)
    {
        static const QString &kDHCPTag = "dhcp";
        static const QString &kGateWayTag = "gateway";
        static const QString &kIpAddressTag = "ipAddr";
        static const QString &kMacAddressTag = "mac";
        static const QString &kNameTag = "name";
        static const QString &kMaskTag = "netMask";
        static const QString &kDnsTag = "dns_servers";

        const QJsonObject &ifObject = interface.toObject();
        if (!ifObject.contains(kDHCPTag) || !ifObject.contains(kGateWayTag)
            || !ifObject.contains(kIpAddressTag) || !ifObject.contains(kMacAddressTag)
            || !ifObject.contains(kNameTag) || !ifObject.contains(kMaskTag)
            || !ifObject.contains(kDnsTag))
        {
            return false;
        }

        const InterfaceInfo &ifInfo = InterfaceInfo(
            ifObject.value(kNameTag).toString()
            , ifObject.value(kIpAddressTag).toString()
            , ifObject.value(kMacAddressTag).toString()
            , ifObject.value(kMaskTag).toString()
            , ifObject.value(kGateWayTag).toString()
            , ifObject.value(kDnsTag).toString().split(' ').front()
            , ifObject.value(kDHCPTag).toBool() ? Qt::Checked : Qt::Unchecked);
        interfaces.push_back(ifInfo);
    }

    extraInfo.interfaces = interfaces;
    return true;
}

///

static bool parseGetTimeCmd(const QJsonObject &object
    , ExtraServerInfo &extraInfo)
{
    if (isErrorReply(object))
        return false;

    const QJsonObject &timeData = extractReplyBody(object).toObject();
    if (timeData.isEmpty())
        return false;

    static const QString kUtcTimeTag = "utcTime";
    static const QString kTimeZoneIdTag = "timezoneId";
    static const QString kTimeZoneOffsetTag = "timeZoneOffset";

    const bool containsTimeZoneId = timeData.contains(kTimeZoneIdTag);
    if (!timeData.contains(kUtcTimeTag)
        || (!timeData.contains(kTimeZoneIdTag) && !timeData.contains(kTimeZoneOffsetTag)))
        return false;

    const QByteArray &timeZoneId = containsTimeZoneId
        ? timeData.value(kTimeZoneIdTag).toString().toUtf8()
        : QTimeZone(timeData.value(kTimeZoneOffsetTag).toInt()).id();

    const qint64 msecs = timeData.value(kUtcTimeTag).toString().toLongLong();

    extraInfo.timeZoneId = timeZoneId;
    extraInfo.utcDateTimeMs = msecs;
    extraInfo.timestampMs = QDateTime::currentMSecsSinceEpoch();

    return true;
}

///

static ExtraServerInfo extractScriptInfoFromAnswer(const QJsonObject &object)
{
    typedef QMultiMap<QString, Constants::SystemCommand> ScriptCmdMap;
    static ScriptCmdMap kMapping = []() -> ScriptCmdMap
    {
        ScriptCmdMap result;
        result.insert("reboot", Constants::SystemCommand::RebootCmd);
        result.insert("restore", Constants::SystemCommand::FactoryDefaultsCmd);
        result.insert("restore_keep_ip", Constants::SystemCommand::FactoryDefaultsButNetworkCmd);
        return result;
    }();

    ExtraServerInfo result;

    const QJsonArray &scripts = extractReplyBody(object).toArray();
    for(const QJsonValue &script: scripts)
    {
        const auto scriptFullName = script.toString();

        /// Scripts could have different extensions in the different OS.
        const auto scriptId = QFileInfo(scriptFullName).completeBaseName();
        const auto range = kMapping.equal_range(scriptId);
        if (range.first != kMapping.end())
        {
            std::for_each(range.first, range.second
                , [&result, &scriptFullName](ScriptCmdMap::mapped_type sysCmd)
            {
                result.sysCommands |= sysCmd;
                result.scriptNames[sysCmd] = scriptFullName;
            });
        }
    }

    result.sysCommands |= Constants::RestartServerCmd;
    return result;
}

static RestClient::Request makeRequest(
    const BaseServerInfoPtr& baseInfo, const QString& password,
    const QString& path, const QUrlQuery& params, qint64 timeoutMs,
    const RestClient::SuccessCallback& successCallback,
    const RestClient::ErrorCallback& errorCallback)
{
    RestClient::HttpAccessMethod httpAccessMethod;
    switch (baseInfo->httpAccessMethod)
    {
        case HttpAccessMethod::kTcp:
            httpAccessMethod = RestClient::HttpAccessMethod::kTcp;
            break;
        case HttpAccessMethod::kUdp:
            httpAccessMethod = RestClient::HttpAccessMethod::kUdp;
            break;
        default:
            Q_ASSERT(false);
            httpAccessMethod = RestClient::HttpAccessMethod::kTcp;
    }

    return RestClient::Request(
        baseInfo->id, baseInfo->hostAddress, baseInfo->port, httpAccessMethod,
        password, path, params, timeoutMs, successCallback, errorCallback);
}

} // namespace

////////////////////////////////////////////////////////////////////////////////

void Client::acceptNewHttpAccessMethod(
    RestClient::NewHttpAccessMethod newHttpAccessMethod,
    const BaseServerInfoPtr& baseInfo)
{
    HttpAccessMethod httpAccessMethod;

    switch (newHttpAccessMethod)
    {
        case RestClient::NewHttpAccessMethod::kTcp:
            httpAccessMethod = HttpAccessMethod::kTcp;
            break;
        case RestClient::NewHttpAccessMethod::kUdp:
            httpAccessMethod = HttpAccessMethod::kUdp;
            break;
        case RestClient::NewHttpAccessMethod::kUnchanged:
            // Nothing to be done.
            return;
        default:
            Q_ASSERT(false);
            return;
    }

    emit instance().accessMethodChanged(baseInfo->id, httpAccessMethod);
    baseInfo->httpAccessMethod = httpAccessMethod;
}

void Client::getScriptInfos(
    const BaseServerInfoPtr &baseInfo,
    const QString &password,
    const Client::ExtraServerInfoSuccessCallback &successCallback,
    const Client::OperationCallback &failedCallback,
    qint64 timeoutMs)
{
    const RestClient::ErrorCallback restClientErrorCallback =
        [failedCallback, baseInfo](
            RestClient::ResultCode, RestClient::NewHttpAccessMethod newHttpAccessMethod)
        {
            acceptNewHttpAccessMethod(newHttpAccessMethod, baseInfo);

            if (failedCallback)
            {
                failedCallback(Client::ResultCode::kInternalAppError
                    , Client::kNoEntitiesAffected);
            }
        };

    if (baseInfo->version < kExtraAbilitiesMinVersion)
    {
        if (failedCallback)
        {
            failedCallback(Client::ResultCode::kInternalAppError
                , Client::kNoEntitiesAffected);
        }
        return;
    }

    const auto id = baseInfo->id;
    const auto restClientSuccessCallback =
        [successCallback, failedCallback, baseInfo](
            const QByteArray &data, RestClient::NewHttpAccessMethod newHttpAccessMethod)
        {
            acceptNewHttpAccessMethod(newHttpAccessMethod, baseInfo);

            const QJsonObject &object = QJsonDocument::fromJson(data).object();
            if (isErrorReply(object))
            {
                if (failedCallback)
                {
                    failedCallback(Client::ResultCode::kInternalAppError
                        , Client::kNoEntitiesAffected);
                }
                return;
            }

            if (successCallback)
                successCallback(baseInfo->id, extractScriptInfoFromAnswer(object));
        };


    instance().m_restClient->sendGet(makeRequest(baseInfo
        , password, kScriptsList, QUrlQuery(), timeoutMs
        , restClientSuccessCallback, restClientErrorCallback));
}

RestClient::SuccessCallback Client::makeSuccessCallbackEx(
    const Client::ExtraServerInfoSuccessCallback& successful,
    const Client::OperationCallback& failed,
    const QString& password,
    Client::AffectedEntities affected,
    const ParseFunction& parser,
    const BaseServerInfoPtr& baseInfo)
{
    return
        [successful, failed, baseInfo, password, affected, parser](
            const QByteArray &data, RestClient::NewHttpAccessMethod newHttpAccessMethod)
        {
            acceptNewHttpAccessMethod(newHttpAccessMethod, baseInfo);

            ExtraServerInfo extraInfo;
            const bool parsed = parser(QJsonDocument::fromJson(data.data()).object(), extraInfo);

            if (parsed)
            {
                if (successful)
                {
                    extraInfo.password = password;
                    successful(baseInfo->id, extraInfo);
                }
            }
            else if (failed)
            {
                failed(Client::ResultCode::kUnspecified, affected);
            }
        };
}

RestClient::SuccessCallback Client::makeSuccessCallbackEx(
    const Client::OperationCallbackEx& callback,
    Client::AffectedEntities affected,
    const BaseServerInfoPtr& baseInfo)
{
    return
        [callback, affected, baseInfo](
            const QByteArray &data, RestClient::NewHttpAccessMethod newHttpAccessMethod)
        {
            acceptNewHttpAccessMethod(newHttpAccessMethod, baseInfo);

            if (!callback)
                return;

            const QJsonObject object = QJsonDocument::fromJson(data.data()).object();
            const auto code = (isErrorReply(object)
                ? Client::ResultCode::kInvalidParam : Client::ResultCode::kSuccess);

            const bool restart = needRestart(object);
            callback(code, affected, restart);
        };
}

RestClient::SuccessCallback Client::makeSuccessCallback(
    const Client::OperationCallback& callback,
    Client::AffectedEntities affected,
    const BaseServerInfoPtr& baseInfo)
{
    return
        [callback, affected, baseInfo](
            const QByteArray &data, RestClient::NewHttpAccessMethod newHttpAccessMethod)
        {
            acceptNewHttpAccessMethod(newHttpAccessMethod, baseInfo);

            if (!callback)
                return;

            const QJsonObject object = QJsonDocument::fromJson(data.data()).object();
            const auto code = (isErrorReply(object)
                ? Client::ResultCode::kInvalidParam : Client::ResultCode::kSuccess);

            callback(code, affected);
        };
}

RestClient::ErrorCallback Client::makeErrorCallbackEx(
    const Client::OperationCallbackEx& callback,
    Client::AffectedEntities affected,
    const BaseServerInfoPtr& baseInfo)
{
    return
        [callback, affected, baseInfo](
            RestClient::ResultCode restResult, RestClient::NewHttpAccessMethod newHttpAccessMethod)
        {
            acceptNewHttpAccessMethod(newHttpAccessMethod, baseInfo);

            if (callback)
                callback(convertRestClientRequestResult(restResult), affected, false);
        };
}

RestClient::ErrorCallback Client::makeErrorCallback(
    const Client::OperationCallback& callback,
    Client::AffectedEntities affected,
    const BaseServerInfoPtr& baseInfo)
{
    return
        [callback, affected, baseInfo](
            RestClient::ResultCode restResult, RestClient::NewHttpAccessMethod newHttpAccessMethod)
        {
            acceptNewHttpAccessMethod(newHttpAccessMethod, baseInfo);

            if (callback)
                callback(convertRestClientRequestResult(restResult), affected);
        };
}

void Client::checkAuth(const BaseServerInfoPtr& baseInfo,
    const QString& password,
    const Client::OperationCallback& callback,
    qint64 timeoutMs)
{
    static const Client::AffectedEntities affected = Client::kNoEntitiesAffected;

    instance().m_restClient->sendGet(makeRequest(baseInfo,
        password, kModuleInfoAuthCommand, QUrlQuery(), timeoutMs,
        makeSuccessCallback(callback, affected, baseInfo),
        makeErrorCallback(callback, affected, baseInfo)));
}

////////////////////////////////////////////////////////////////////////////////

bool Client::parseModuleInformationReply(const QJsonObject &reply
    , BaseServerInfo *baseInfo)
{
    Q_ASSERT(baseInfo);

    const QStringList &keys = reply.keys();
    for (const auto &key: keys)
    {
        const auto itHandler = parser.find(key);
        if (itHandler != parser.end())
            (*itHandler)(reply, baseInfo);
    }

    if (baseInfo->version < kExtraAbilitiesMinVersion)
    {
        /// Assume all versions less than kExtraAbilitiesMinVersion have hdd due
        /// to we can't discover state through the rest api
        baseInfo->flags |= Constants::HasHdd;
    }
    /*
    else
        baseInfo.safeMode = true;   /// for debug!
    */

    /// Before conversion baseInfo.os contains OS caps but not finalized OS/Type name
    baseInfo->os = convertCapsToReadable(baseInfo->customization, baseInfo->os);

    return true;
}

///

const QStringList &Client::defaultAdminPasswords()
{
    static auto result = QStringList()
        << QStringLiteral("admin") << QStringLiteral("123");
    return result;
}

void Client::multicastModuleInformation(const QUuid &id
    , const BaseServerInfoCallback &successCallback
    , const OperationCallback &failedCallback
    , qint64 timeoutMs)
{
    static const Client::AffectedEntities affected = Client::AffectedEntities(
            Client::kAllEntitiesAffected & ~Client::kAllAddressFlagsAffected);

    static const QString kReplyTag = "reply";

    BaseServerInfoPtr baseInfo = std::make_shared<BaseServerInfo>();
    baseInfo->id = id;
    baseInfo->httpAccessMethod = HttpAccessMethod::kUdp; //< Force multicast usage.

    const auto restClientSuccessCallback =
        [successCallback, failedCallback, baseInfo](
            const QByteArray &data, RestClient::NewHttpAccessMethod newHttpAccessMethod)
        {
            acceptNewHttpAccessMethod(newHttpAccessMethod, baseInfo);

            BaseServerInfo resultInfo;
            resultInfo.id = baseInfo->id;

            const QJsonObject object = QJsonDocument::fromJson(data.data()).object();
            if (object.isEmpty() || isErrorReply(object) || !object.keys().contains(kReplyTag)
                || !parseModuleInformationReply(object.value(kReplyTag).toObject(), &resultInfo))
            {
                if (failedCallback)
                    failedCallback(ResultCode::kUnspecified, affected);
                return;
            }

            if (successCallback)
                successCallback(&resultInfo);
        };

    const QString &kModuleInformationCommand = kApiNamespaceTag + "moduleInformation";
    instance().m_restClient->sendGet(makeRequest(baseInfo
        , QString(), kModuleInformationCommand, QUrlQuery()
        , timeoutMs, restClientSuccessCallback, makeErrorCallback(failedCallback, affected, baseInfo)));
}

///

void Client::getTime(const BaseServerInfoPtr &baseInfo
    , const QString &password
    , const ExtraServerInfoSuccessCallback &successful
    , const OperationCallback &failed
    , qint64 timeoutMs)
{
    static const Client::AffectedEntities affected =
        (Client::kTimeZoneAffected | Client::kDateTimeAffected);

    QUrlQuery query;
    if (baseInfo->flags & Constants::AllowChangeDateTimeFlag)
        query.addQueryItem(kLocalTimeFlagTag, QString());

    const auto &parser = [](const QJsonObject &object, ExtraServerInfo &extraInfo)
        { return parseGetTimeCmd(object, extraInfo); };

    const auto successCallback = makeSuccessCallbackEx(
        successful, failed, password, affected, parser, baseInfo);

    instance().m_restClient->sendGet(makeRequest(baseInfo
        , password, kGetTimeCommand, query,  timeoutMs
        , successCallback, makeErrorCallback(failed, affected, baseInfo)));
}

///

void Client::getIfList(const BaseServerInfoPtr &baseInfo
    , const QString &password
    , const ExtraServerInfoSuccessCallback &successful
    , const OperationCallback &failed
    , qint64 timeoutMs)
{
    static const Client::AffectedEntities affected = Client::kAllAddressFlagsAffected;

    const auto &parser = [](const QJsonObject &object, ExtraServerInfo &extraInfo)
        { return parseIfListCmd(object, extraInfo); };

    const auto &ifListSuccessful = makeSuccessCallbackEx(
        successful, failed, password, affected, parser, baseInfo);

    instance().m_restClient->sendGet(makeRequest(baseInfo
        , password, kIfListCommand, QUrlQuery(), timeoutMs
        , ifListSuccessful, makeErrorCallback(failed, affected, baseInfo)));
}

///

void Client::getServerExtraInfo(const BaseServerInfoPtr &baseInfo
    , const QString &password
    , const ExtraServerInfoSuccessCallback &successful
    , const OperationCallback &failed
    , qint64 timeoutMs)
{
    static const Client::AffectedEntities affected = (Client::kAllAddressFlagsAffected
        | Client::kTimeZoneAffected | Client::kDateTimeAffected);

    typedef std::shared_ptr<ExtraServerInfo> ExtraServerInfoPtr;
    const ExtraServerInfoPtr extraInfoStorage(new ExtraServerInfo());

    const auto &getTimeFailed = [failed](const ResultCode errorCode, Client::AffectedEntities /* affectedEntities */)
    {
        if (failed)
            failed(errorCode, affected);
    };

    const auto &getTimeSuccessful = [extraInfoStorage, baseInfo, password, successful, timeoutMs]
        (const QUuid &id, const ExtraServerInfo &extraInfo)
    {
        /// getTime command is successfully executed up to now

        extraInfoStorage->timeZoneId = extraInfo.timeZoneId;
        extraInfoStorage->timestampMs = extraInfo.timestampMs;
        extraInfoStorage->utcDateTimeMs = extraInfo.utcDateTimeMs;

        enum { kParallelCommandsCount = 2 };
        const std::shared_ptr<int> commandsLeft(new int(kParallelCommandsCount));

        const auto condCallSuccessful = [commandsLeft, successful]
            (const QUuid &id, const ExtraServerInfo &info)
        {
            if ((--(*commandsLeft) == 0) && successful)
                successful(id, info);
        };

        const auto &extraCommandsFailed =
            [extraInfoStorage, condCallSuccessful, id](
                const ResultCode /* errorCode */, Client::AffectedEntities /* affectedEntities */)
            {
                /// Calls successful, anyway, for old servers not supporting ifList
                condCallSuccessful(id, *extraInfoStorage);
            };

        const auto &ifListSuccessful =
            [extraInfoStorage, condCallSuccessful](
                const QUuid &id, const ExtraServerInfo &extraInfo)
            {
                extraInfoStorage->interfaces = extraInfo.interfaces;
                condCallSuccessful(id, *extraInfoStorage);
            };

        const auto &extraFlagsSuccessful =
            [extraInfoStorage, condCallSuccessful](
                const QUuid &id, const ExtraServerInfo &extraInfo)
            {
                extraInfoStorage->sysCommands = extraInfo.sysCommands;
                extraInfoStorage->scriptNames = extraInfo.scriptNames;
                condCallSuccessful(id, *extraInfoStorage);
            };

        getIfList(baseInfo, password, ifListSuccessful, extraCommandsFailed, timeoutMs);
        getScriptInfos(baseInfo, password, extraFlagsSuccessful, extraCommandsFailed, timeoutMs);
    };

    const auto &callback = [extraInfoStorage, failed, baseInfo, password
        , getTimeSuccessful, getTimeFailed, timeoutMs]
        (const ResultCode errorCode, Client::AffectedEntities /* affectedEntities */)
    {
        if (errorCode != ResultCode::kSuccess)
        {
            if (failed)
                failed(errorCode, affected);;
            return;
        }

        extraInfoStorage->password = password;
        getTime(baseInfo, password, getTimeSuccessful, getTimeFailed, timeoutMs);
    };

    return checkAuth(baseInfo, password, callback, timeoutMs);
}

///

void Client::sendIfListRequest(const BaseServerInfoPtr &baseInfo
    , const QString &password
    , const ExtraServerInfoSuccessCallback &successful
    , const OperationCallback &failed
    , qint64 timeoutMs)
{
    if (!(baseInfo->flags & Constants::AllowIfConfigFlag))
    {
        if (failed)
            failed(ResultCode::kUnspecified, Client::kAllEntitiesAffected);
        return;
    }

    const auto &parser = [](const QJsonObject &object, ExtraServerInfo &extraInfo)
        { return parseIfListCmd(object, extraInfo); };
    const auto &localSuccessful = makeSuccessCallbackEx(successful, failed,
        password, Client::kAllAddressFlagsAffected, parser, baseInfo);
    instance().m_restClient->sendGet(makeRequest(baseInfo
        , password, kIfListCommand, QUrlQuery(), timeoutMs
        , localSuccessful, makeErrorCallback(failed, Client::kAllAddressFlagsAffected, baseInfo)));
}

///

void Client::sendRestartRequest(const BaseServerInfoPtr &baseInfo
    , const QString &password
    , const OperationCallback &callback)
{
    instance().m_restClient->sendGet(makeRequest(baseInfo
        , password, kRestartCommand
        , QUrlQuery(), kDefaultTimeoutMs
        , makeSuccessCallback(callback, Client::kNoEntitiesAffected, baseInfo)
        , makeErrorCallback(callback, Client::kNoEntitiesAffected, baseInfo)));
}

void Client::sendSystemCommand(const BaseServerInfoPtr &baseInfo
    , const QString &password
    , const Constants::SystemCommand sysCommand
    , const OperationCallback &callback)
{
    QString cmd;
    switch(sysCommand)
    {
    case Constants::SystemCommand::RestartServerCmd:
        sendRestartRequest(baseInfo, password, callback);
        return;

    case Constants::SystemCommand::RebootCmd:
        cmd = QStringLiteral("reboot");
        break;
    case Constants::SystemCommand::FactoryDefaultsCmd:
        cmd = QStringLiteral("restore");
        break;
    case Constants::SystemCommand::FactoryDefaultsButNetworkCmd:
        cmd = QStringLiteral("restore_keep_ip");
        break;
    default:
    {
        if (callback)
            callback(ResultCode::kInternalAppError, Client::kNoEntitiesAffected);
        return;
    }

    };

    static const auto kExecutePathTemplate = QStringLiteral("%1/%2").arg(kExecuteCommand);

    const auto path = kExecutePathTemplate.arg(cmd);
    instance().m_restClient->sendGet(makeRequest(baseInfo
        , password, path, QUrlQuery(), kDefaultTimeoutMs
        , makeSuccessCallback(callback, Client::kNoEntitiesAffected, baseInfo)
        , makeErrorCallback(callback, Client::kNoEntitiesAffected, baseInfo)));
}

///

void Client::sendSetTimeRequest(const BaseServerInfoPtr &baseInfo
    , const QString &password
    , qint64 utcDateTimeMs
    , const QByteArray &timeZoneId
    , const OperationCallbackEx &callback)
{
    static const Client::AffectedEntities affected = (Client::kDateTimeAffected | Client::kTimeZoneAffected);
    if (utcDateTimeMs <= 0 || !QTimeZone(timeZoneId).isValid())
    {
        if (callback)
            callback(ResultCode::kUnspecified, affected, false);
        return;
    }

    static const QString &kDateTimeTag = "datetime";
    static const QString &kTimeZoneTag = "timezone";

    QUrlQuery query;
    query.addQueryItem(kDateTimeTag, QString::number(utcDateTimeMs));
    query.addQueryItem(kTimeZoneTag, timeZoneId);

    enum { kSpecialTimeoutMs = 30 * 1000 }; /// Due to server could apply time changes too long in some cases (Nx1 for example)
    instance().m_restClient->sendGet(makeRequest(baseInfo
        , password, kSetTimeCommand, query, kSpecialTimeoutMs
        , makeSuccessCallbackEx(callback, affected, baseInfo)
        , makeErrorCallbackEx(callback, affected, baseInfo)));
}

///
void Client::sendSetSystemNameRequest(const BaseServerInfoPtr &baseInfo
    , const QString &password
    , const QString &systemName
    , const OperationCallbackEx &callback)
{
    if (systemName.isEmpty())
    {
        if (callback)
            callback(ResultCode::kUnspecified, Client::kSystemNameAffected, false);
        return;
    }

    static const QString kSystemNameTag = "systemName";
    static const QString oldPasswordTag = "oldPassword";

    QUrlQuery query;
    query.addQueryItem(kSystemNameTag, systemName);
    query.addQueryItem(oldPasswordTag, password);

    instance().m_restClient->sendGet(makeRequest(baseInfo
        , password, kConfigureCommand, query, kDefaultTimeoutMs
        , makeSuccessCallbackEx(callback, Client::kSystemNameAffected, baseInfo)
        , makeErrorCallbackEx(callback, Client::kSystemNameAffected, baseInfo)));
}

///

void Client::sendSetPasswordRequest(const BaseServerInfoPtr &baseInfo
    , const QString &currentPassword
    , const QString &password
    , bool useNewPassword
    , const OperationCallbackEx &callback)
{
    if (password.isEmpty())
    {
        if (callback)
            callback(ResultCode::kUnspecified, Client::kPasswordAffected, false);

        return;
    }

    static const QString newPasswordTag = "password";
    static const QString oldPasswordTag = "oldPassword";

    const QString authPass = (useNewPassword ? password : currentPassword);
    QUrlQuery query;
    query.addQueryItem(newPasswordTag, password);
    query.addQueryItem(oldPasswordTag, authPass);

    instance().m_restClient->sendGet(makeRequest(baseInfo
        , authPass, kConfigureCommand, query, kDefaultTimeoutMs
        , makeSuccessCallbackEx(callback, Client::kPasswordAffected, baseInfo)
        , makeErrorCallbackEx(callback, Client::kPasswordAffected, baseInfo)));
}

///

void Client::sendSetPortRequest(const BaseServerInfoPtr &baseInfo
    , const QString &password
    , int port
    , const OperationCallbackEx &callback)
{
    if (!port)
    {
        if (callback)
            callback(ResultCode::kUnspecified, Client::kPortAffected, false);
       return;
    }

    static const QString oldPasswordTag = "oldPassword";

    QUrlQuery query;
    query.addQueryItem(kPortTag, QString::number(port));
    query.addQueryItem(oldPasswordTag, password);

    instance().m_restClient->sendGet(makeRequest(baseInfo
        , password, kConfigureCommand, query, kDefaultTimeoutMs
        , makeSuccessCallbackEx(callback, Client::kPortAffected, baseInfo)
        , makeErrorCallbackEx(callback, Client::kPortAffected, baseInfo)));
}

void Client::sendChangeItfRequest(const BaseServerInfoPtr &baseInfo
    , const QString &password
    , const ItfUpdateInfoContainer &updateInfos
    , const OperationCallbackEx &callback)
{
    QJsonArray jsonInfoChanges;
    Client::AffectedEntities affected = 0;
    for (const ItfUpdateInfo &change: updateInfos)
    {
        static const QString kIpTag = "ipAddr";
        static const QString kMaskTag = "netMask";
        static const QString kDHCPFlagTag = "dhcp";
        static const QString kNameTag = "name";
        static const QString kDnsTag = "dns_servers";
        static const QString kGatewayTag = "gateway";

        QJsonObject jsonInfoChange;
        if (!change.name.isEmpty())
            jsonInfoChange.insert(kNameTag, change.name);

        if (change.useDHCP)
        {
            jsonInfoChange.insert(kDHCPFlagTag, *change.useDHCP);
            affected |= Client::kDHCPUsageAffected;
        }

        if (change.ip && !change.ip->isEmpty())
        {
            jsonInfoChange.insert(kIpTag, *change.ip);
            affected |= Client::kIpAddressAffected;
        }

        if (change.mask && !change.mask->isEmpty())
        {
            jsonInfoChange.insert(kMaskTag, *change.mask);
            affected |= Client::kMaskAffected;
        }

        if (change.dns)
        {
            jsonInfoChange.insert(kDnsTag, *change.dns);
            affected |= Client::kDNSAffected;
        }

        if (change.gateway)
        {
            jsonInfoChange.insert(kGatewayTag, *change.gateway);
            affected |= Client::kGatewayAffected;
        }

        jsonInfoChanges.append(jsonInfoChange);
    }

    instance().m_restClient->sendPost(
        makeRequest(baseInfo,
            password, kIfConfigCommand, QUrlQuery(),
            kDefaultTimeoutMs,
            makeSuccessCallbackEx(callback, affected, baseInfo),
            makeErrorCallbackEx(callback, affected, baseInfo)),
        QJsonDocument(jsonInfoChanges).toJson());
}

///

Client::ItfUpdateInfo::ItfUpdateInfo()
    : name()
    , useDHCP()
    , ip()
    , mask()
    , dns()
    , gateway()
{
}

Client::ItfUpdateInfo::ItfUpdateInfo(const ItfUpdateInfo &other)
    : name(other.name)
    , useDHCP(other.useDHCP ? new bool (*other.useDHCP) : nullptr)
    , ip(other.ip ? new QString(*other.ip) : nullptr)
    , mask(other.mask ? new QString(*other.mask) : nullptr)
    , dns(other.dns ? new QString(*other.dns) : nullptr)
    , gateway(other.gateway ? new QString(*other.gateway) : nullptr)
{
}

Client::ItfUpdateInfo::ItfUpdateInfo(const QString &initName)
    : name(initName)
    , useDHCP()
    , ip()
    , mask()
    , dns()
    , gateway()
{
}

Client::ItfUpdateInfo &Client::ItfUpdateInfo::operator =(const ItfUpdateInfo &other)
{
    name = other.name;
    useDHCP = std::shared_ptr<bool>(other.useDHCP ? new bool (*other.useDHCP) : nullptr);
    ip = std::shared_ptr<QString>(other.ip ? new QString(*other.ip) : nullptr);
    mask = std::shared_ptr<QString>(other.mask ? new QString(*other.mask) : nullptr);
    dns = std::shared_ptr<QString>(other.dns ? new QString(*other.dns) : nullptr);
    gateway = std::shared_ptr<QString>(other.gateway ? new QString(*other.gateway) : nullptr);
    return *this;
}

Client::Client()
    : m_restClient(new RestClient(kUserAgent, kAdminUserName))
{
}

Client::~Client()
{
}

Client& Client::instance()
{
    static Client client;
    return client;
}

} // namespace api
} // namespace mediaserver
} // namespace nx
