
#include "requests.h"

#include <QUrl>
#include <QUrlQuery>

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

#include <base/server_info.h>
#include <helpers/rest_client.h>
#include <helpers/time_helper.h>

namespace 
{
    const QString kApiNamespaceTag = "/api/";
    
    const QString kModuleInfoAuthCommand = kApiNamespaceTag + "moduleInformationAuthenticated";
    const QString kConfigureCommand = kApiNamespaceTag + "configure";
    const QString kIfConfigCommand = kApiNamespaceTag + "ifconfig";
    const QString kSetTimeCommand = kApiNamespaceTag + "settime";
    const QString kGetTimeCommand = kApiNamespaceTag + "gettime";
    const QString kIfListCommand = kApiNamespaceTag + "iflist";
    const QString kRestartCommand = kApiNamespaceTag + "restart";
    const QString kScriptsList = kApiNamespaceTag + "";

    const QString kIDTag = "id";
    const QString kSeedTag = "seed";
    const QString kRuntimeIdTag = "runtimeId";
    const QString kVersionTag = "version";
    const QString kNameTag = "name";
    const QString kSystemNameTag = "systemName";
    const QString kPortTag = "port";
    const QString kFlagsTag = "flags";
    const QString kLocalTimeFlagTag = "local";

    QUrl makeUrl(const QString &host
        , const int port
        , const QString &password
        , const QString &command)

    {
        QUrl url;
        url.setScheme("http");
        url.setHost(host);
        url.setPort(port);
        url.setUserName(rtu::RestClient::defaultAdminUserName());
        url.setPassword(password);
        url.setPath(command);
        return url;
    }

    QUrl makeUrl(const rtu::ServerInfo &info
        , const QString &command)
    {
        if (!info.hasExtraInfo())
            return QUrl();

        return makeUrl(info.baseInfo().hostAddress, info.baseInfo().port
            , info.extraInfo().password, command);
    }
    
    typedef QHash<QString, std::function<void (const QJsonObject &object
        , rtu::BaseServerInfo &info)> > KeyParserContainer;
    const KeyParserContainer parser = []() -> KeyParserContainer
    {
        KeyParserContainer result;
        result.insert(kIDTag, [](const QJsonObject& object, rtu::BaseServerInfo &info)
            { info.id = QUuid(object.value(kIDTag).toString()); });
        result.insert(kSeedTag, [](const QJsonObject& object, rtu::BaseServerInfo &info)
            { info.id = QUuid(object.value(kSeedTag).toString()); });
        result.insert(kRuntimeIdTag, [](const QJsonObject& object, rtu::BaseServerInfo &info)
            { info.runtimeId = QUuid(object.value(kRuntimeIdTag).toString()); });
        result.insert(kNameTag, [](const QJsonObject& object, rtu::BaseServerInfo &info)
            { info.name = object.value(kNameTag).toString(); });
        result.insert(kSystemNameTag, [](const QJsonObject& object, rtu::BaseServerInfo &info)
            { info.systemName = object.value(kSystemNameTag).toString(); });
        result.insert(kPortTag, [](const QJsonObject& object, rtu::BaseServerInfo &info)
            { info.port = object.value(kPortTag).toInt(); });
        result.insert(kVersionTag, [](const QJsonObject& object, rtu::BaseServerInfo &info)
            { info.version = object.value(kVersionTag).toString(); });

        result.insert(kFlagsTag, [](const QJsonObject& object, rtu::BaseServerInfo &info)
            {
                typedef QPair<QString, rtu::Constants::ServerFlag> TextFlagsInfo;
                static const TextFlagsInfo kKnownFlags[] =
                {
                    TextFlagsInfo("SF_timeCtrl", rtu::Constants::ServerFlag::AllowChangeDateTimeFlag)
                    , TextFlagsInfo("SF_IfListCtrl", rtu::Constants::ServerFlag::AllowIfConfigFlag)
                    , TextFlagsInfo("SF_AutoSystemName", rtu::Constants::ServerFlag::IsFactoryFlag)
                    , TextFlagsInfo("SF_Has_HDD", rtu::Constants::ServerFlag::HasHdd)
                };

                info.flags = rtu::Constants::ServerFlag::NoFlags;
                const QString textFlags = object.value(kFlagsTag).toString();
                for (const TextFlagsInfo &flagInfo: kKnownFlags)
                {
                    if (textFlags.contains(flagInfo.first))
                        info.flags |= flagInfo.second;
                }
            });
        return result;
    }();
}

namespace /// Parsers stuff
{
    bool isErrorReply(const QJsonObject &object)
    {
        static const QString &kErrorTag = "error";
        if (!object.contains(kErrorTag))
            return false;

        return (object.value(kErrorTag).toInt() != 0);
    }
  
    typedef std::function<bool (const QJsonObject &object
        , rtu::ExtraServerInfo &extraInfo)> ParseFunction;

    rtu::RestClient::SuccessCallback makeReplyCallbackEx(
        const rtu::ExtraServerInfoSuccessCallback &successful
        , const rtu::OperationCallback &failed
        , const QUuid &id
        , const QString &password
        , rtu::Constants::AffectedEntities affected
        , const ParseFunction &parser)
    {
        const auto &result = [successful, failed, id, password, affected, parser](const QByteArray &data)
        {
            rtu::ExtraServerInfo extraInfo;
            const bool parsed = parser(QJsonDocument::fromJson(data.data()).object(), extraInfo);
                
            if (parsed)
            {
                if (successful)
                {
                    extraInfo.password = password;
                    successful(id, extraInfo);
                }
            }
            else if (failed)
            {
                failed(rtu::RequestError::kUnspecified, affected);
            }
        };

        return result;
    }

    ///

    QJsonValue extractReplyBody(const QJsonObject &object)
    {
        static const QString &kReplyTag = "reply";
        return (object.contains(kReplyTag) ? object.value(kReplyTag) : QJsonValue());
    }
    ///

    bool parseIfListCmd(const QJsonObject &object
        , rtu::ExtraServerInfo &extraInfo)
    {
        if (isErrorReply(object))
            return false;

        const QJsonArray &ifArray = extractReplyBody(object).toArray();
        if (ifArray.isEmpty())
            return false;
    
        rtu::InterfaceInfoList interfaces;
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
        
            const rtu::InterfaceInfo &ifInfo = rtu::InterfaceInfo(
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

    bool parseGetTimeCmd(const QJsonObject &object
        , rtu::ExtraServerInfo &extraInfo)
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

    rtu::RestClient::SuccessCallback makeSuccessCallback(const rtu::OperationCallback &callback
        , rtu::Constants::AffectedEntities affected)
    {
        return [callback, affected](const QByteArray &data)
        {
            if (!callback)
                return;

            const QJsonObject object = QJsonDocument::fromJson(data.data()).object();
            const auto code = (isErrorReply(object) 
                ? rtu::RequestError::kInternalAppError : rtu::RequestError::kSuccess);

            callback(code, affected);
        };
    }

    rtu::RestClient::ErrorCallback makeErrorCallback(const rtu::OperationCallback &callback
        , rtu::Constants::AffectedEntities affected)
    {
        return [callback, affected](rtu::RequestError error)
        {
            if (callback)
                callback(error, affected);
        };
    }

    ///

    void checkAuth(const rtu::BaseServerInfoPtr &baseInfo
        , const QString &password
        , const rtu::OperationCallback &callback
        , int timeout = rtu::RestClient::kUseStandardTimeout)
    {
        static const rtu::Constants::AffectedEntities affected = rtu::Constants::kNoEntitiesAffected;

        const rtu::RestClient::Request request(baseInfo
            , password, kModuleInfoAuthCommand, QUrlQuery(), timeout
            , makeSuccessCallback(callback, affected), makeErrorCallback(callback, affected));
        rtu::RestClient::sendGet(request);
    }

    ///

    rtu::ExtraServerInfo extractExtraFlagsFromAnswer(const QJsonObject &object)
    {
        typedef QMap<QString, rtu::Constants::ExtraServerFlag> ScriptFlagMap;
        static const ScriptFlagMap kMapping = []() -> ScriptFlagMap
        {
            /// TODO: add real script names
            ScriptFlagMap result;
            result.insert("test", rtu::Constants::ExtraServerFlag::OsRestartAbility);
            return result;
        }();

        rtu::ExtraServerInfo result;

        const QJsonArray &scripts = extractReplyBody(object).toArray();
        for(const QJsonValue &script: scripts)
        {
            const auto scriptFullName = script.toString();

            /// Scripts could have different extensions in the different OS. 
            const auto scriptId = QFileInfo(scriptFullName).completeBaseName();

            const auto it = kMapping.find(scriptId);
            if (it != kMapping.end())
            {
                const auto flag = it.value();
                result.extraFlags |= flag;
                result.scriptNames[flag] = scriptFullName;
            }
        }
        return result;
    }

    void getExtraFlags(const rtu::BaseServerInfoPtr &baseInfo
        , const QString &password
        , const rtu::ExtraServerInfoSuccessCallback &successCallback
        , const rtu::OperationCallback &failedCallback
        , const qint64 timeout)
    {
        static const auto minScriptAvailableVersion = QStringLiteral("2.4.1");

        const auto failed = [failedCallback](rtu::RequestError error)
        {
            if (failedCallback)
            {
                failedCallback(rtu::RequestError::kInternalAppError
                    , rtu::Constants::kNoEntitiesAffected);
            }
        };

        if (baseInfo->version < minScriptAvailableVersion)
        {
            failed(rtu::RequestError::kRequestTimeout);
            return;
        }

        const auto id = baseInfo->id;
        const auto success = [id, successCallback, failed](const QByteArray &data)
        {
            const QJsonObject &object = QJsonDocument::fromJson(data).object();
            if (isErrorReply(object))
            {
                failed(rtu::RequestError::kInternalAppError);
                return;
            }

            if (successCallback)
                successCallback(id, extractExtraFlagsFromAnswer(object));
        };

        const rtu::RestClient::Request request(baseInfo, password
            , kScriptsList, QUrlQuery(), timeout, success, failed);
        rtu::RestClient::sendGet(request);
    }
}

///

bool rtu::parseModuleInformationReply(const QJsonObject &reply
    , rtu::BaseServerInfo &baseInfo)
{
    const QStringList &keys = reply.keys();
    for (const auto &key: keys)
    {
        const auto itHandler = parser.find(key);
        if (itHandler != parser.end())
            (*itHandler)(reply, baseInfo);
    }

    return true;
}

///

void rtu::multicastModuleInformation(const QUuid &id
    , const BaseServerInfoCallback &successCallback
    , const OperationCallback &failedCallback
    , int timeout)
{
    static const Constants::AffectedEntities affected = (Constants::kAllEntitiesAffected 
        & ~Constants::kAllAddressFlagsAffected);

    static const QString kReplyTag = "reply";
    
    const auto &successfull = [id, successCallback, failedCallback](const QByteArray &data)
    {
        BaseServerInfo resultInfo;
        resultInfo.id = id;

        const QJsonObject object = QJsonDocument::fromJson(data.data()).object();
        if (object.isEmpty() || isErrorReply(object) || !object.keys().contains(kReplyTag)
            || !parseModuleInformationReply(object.value(kReplyTag).toObject(), resultInfo))
        {
            if (failedCallback)
                failedCallback(RequestError::kUnspecified, affected);
            return;
        }

        if (successCallback)
            successCallback(resultInfo);
    };

    BaseServerInfoPtr info = std::make_shared<BaseServerInfo>();
    info->id = id;
    info->accessibleByHttp = false;  /// force multicast usage

    const QString &kModuleInformationCommand = kApiNamespaceTag + "moduleInformation";
    RestClient::Request request(info, QString(), kModuleInformationCommand, QUrlQuery()
        , timeout, successfull, makeErrorCallback(failedCallback, affected));
    RestClient::sendGet(request);
}

/// 

void rtu::getTime(const BaseServerInfoPtr &baseInfo
    , const QString &password
    , const ExtraServerInfoSuccessCallback &successful
    , const OperationCallback &failed
    , int timeout)
{
    static const Constants::AffectedEntities affected = 
        (Constants::kTimeZoneAffected | Constants::kDateTimeAffected);

    QUrlQuery query;
    if (baseInfo->flags & Constants::AllowChangeDateTimeFlag)
        query.addQueryItem(kLocalTimeFlagTag, QString());

    const auto &parser = [](const QJsonObject &object, ExtraServerInfo &extraInfo)
        { return parseGetTimeCmd(object, extraInfo); };

    const auto &localSuccessful = makeReplyCallbackEx(
        successful, failed, baseInfo->id, password, affected, parser);

    const RestClient::Request request(baseInfo
        , password, kGetTimeCommand, query,  timeout
        , localSuccessful, makeErrorCallback(failed, affected));

    RestClient::sendGet(request);
}

///

void rtu::getIfList(const BaseServerInfoPtr &baseInfo
    , const QString &password
    , const ExtraServerInfoSuccessCallback &successful
    , const OperationCallback &failed
    , int timeout)
{
    static const Constants::AffectedEntities affected = Constants::kAllAddressFlagsAffected;

    const auto &parser = [](const QJsonObject &object, ExtraServerInfo &extraInfo)
        { return parseIfListCmd(object, extraInfo); };

    const auto &ifListSuccessfull = makeReplyCallbackEx(
        successful, failed, baseInfo->id, password, affected, parser);

    const RestClient::Request request(baseInfo
        , password, kIfListCommand, QUrlQuery(),  timeout
        , ifListSuccessfull, makeErrorCallback(failed, affected));
    RestClient::sendGet(request);
}

///

void rtu::getServerExtraInfo(const BaseServerInfoPtr &baseInfo
    , const QString &password
    , const ExtraServerInfoSuccessCallback &successful
    , const OperationCallback &failed
    , int timeout)
{
    static const Constants::AffectedEntities affected = (Constants::kAllAddressFlagsAffected 
        | Constants::kTimeZoneAffected | Constants::kDateTimeAffected);

    typedef std::shared_ptr<ExtraServerInfo> ExtraServerInfoPtr;
    const ExtraServerInfoPtr extraInfoStorage(new ExtraServerInfo());

    const auto &getTimeFailed = [failed](const RequestError errorCode, Constants::AffectedEntities /* affectedEntities */)
    {
        if (failed)
            failed(errorCode, affected);
    };

    const auto &getTimeSuccessfull = [extraInfoStorage, baseInfo, password, successful, timeout]
        (const QUuid &id, const rtu::ExtraServerInfo &extraInfo) 
    {
        /// getTime command is successfully executed up to now

        extraInfoStorage->timeZoneId = extraInfo.timeZoneId;
        extraInfoStorage->timestampMs = extraInfo.timestampMs;
        extraInfoStorage->utcDateTimeMs = extraInfo.utcDateTimeMs;

        enum { kParallelCommandsCount = 2 };
        const IntPointer commandsLeft(new int(kParallelCommandsCount));

        const auto condCallSuccessfull = [commandsLeft, successful]
            (const QUuid &id, const ExtraServerInfo &info)
        {
            if ((--(*commandsLeft) == 0) && successful)
                successful(id, info);
        };

        const auto &extraCommandsFailed = [extraInfoStorage, condCallSuccessfull, id]
            (const RequestError /* errorCode */, Constants::AffectedEntities /* affectedEntities */)
        {
            /// Calls successfull, anyway, for old servers not supporting ifList
            condCallSuccessfull(id, *extraInfoStorage);
        };

        const auto &ifListSuccessful = [extraInfoStorage, condCallSuccessfull]
            (const QUuid &id, const rtu::ExtraServerInfo &extraInfo)
        {
            extraInfoStorage->interfaces = extraInfo.interfaces;
            condCallSuccessfull(id, *extraInfoStorage);
        };

        const auto &extraFlagsSuccessful = [extraInfoStorage, condCallSuccessfull]
            (const QUuid &id, const ExtraServerInfo &extraInfo)
        {
            extraInfoStorage->extraFlags = extraInfo.extraFlags;
            extraInfoStorage->scriptNames = extraInfo.scriptNames;
            condCallSuccessfull(id, *extraInfoStorage);
        };

        getIfList(baseInfo, password, ifListSuccessful, extraCommandsFailed, timeout);
        getExtraFlags(baseInfo, password, extraFlagsSuccessful, extraCommandsFailed, timeout);
    };

    const auto &callback = [extraInfoStorage, failed, baseInfo, password
        , getTimeSuccessfull, getTimeFailed, timeout]
        (const RequestError errorCode, Constants::AffectedEntities /* affectedEntities */)
    {
        if (errorCode != RequestError::kSuccess)
        {
            if (failed)
                failed(errorCode, affected);;
            return;
        }

        extraInfoStorage->password = password;
        getTime(baseInfo, password, getTimeSuccessfull, getTimeFailed, timeout);
    };

    return checkAuth(baseInfo, password, callback, timeout);
}

///

void rtu::sendIfListRequest(const BaseServerInfoPtr &info
    , const QString &password
    , const ExtraServerInfoSuccessCallback &successful
    , const OperationCallback &failed
    , int timeout)
{
    if (!(info->flags & Constants::AllowIfConfigFlag))
    {
        if (failed)
            failed(RequestError::kUnspecified, Constants::kAllEntitiesAffected);
        return;
    }
 
    const auto &parser = [](const QJsonObject &object, ExtraServerInfo &extraInfo)
        { return parseIfListCmd(object, extraInfo); };
    const auto &localSuccessful = makeReplyCallbackEx(successful, failed, info->id
        , password, Constants::kAllAddressFlagsAffected, parser);
    const RestClient::Request request(info, password, kIfListCommand, QUrlQuery(), timeout
        , localSuccessful, makeErrorCallback(failed, Constants::kAllAddressFlagsAffected));
    RestClient::sendGet(request);
}

///

void rtu::sendRestartRequest(const BaseServerInfoPtr &info
    , const QString &password
    , const OperationCallback &callback)
{ 
    const RestClient::Request request(info, password, kRestartCommand
        , QUrlQuery(), RestClient::kUseStandardTimeout
        , makeSuccessCallback(callback, Constants::kNoEntitiesAffected)
        , makeErrorCallback(callback, Constants::kNoEntitiesAffected));

    RestClient::sendGet(request);
}

void rtu::sendCustomCommand(const BaseServerInfoPtr &info
    , const QString &password
    , const QString &path
    , const QUrlQuery &params
    , const OperationCallback &callback)
{
    
//    /const RestClient::Request
}

///

void rtu::sendSetTimeRequest(const BaseServerInfoPtr &baseInfo
    , const QString &password
    , qint64 utcDateTimeMs
    , const QByteArray &timeZoneId
    , const OperationCallback &callback)
{
    static const Constants::AffectedEntities affected = (Constants::kDateTimeAffected | Constants::kTimeZoneAffected);
    if (utcDateTimeMs <= 0 || !QTimeZone(timeZoneId).isValid())
    {
        if (callback)
            callback(RequestError::kUnspecified, affected);
        return;
    }
    
    static const QString &kDateTimeTag = "datetime";
    static const QString &kTimeZoneTag = "timezone";
    
    QUrlQuery query;
    query.addQueryItem(kDateTimeTag, QString::number(utcDateTimeMs));
    query.addQueryItem(kTimeZoneTag, timeZoneId);
    
    enum { kSpecialTimeout = 30 * 1000 }; /// Due to server could apply time changes too long in some cases (Nx1 for example)
    const RestClient::Request request(baseInfo
        , password, kSetTimeCommand, query, kSpecialTimeout
        , makeSuccessCallback(callback, affected), makeErrorCallback(callback, affected)); 
    RestClient::sendGet(request);
}

///
void rtu::sendSetSystemNameRequest(const BaseServerInfoPtr &baseInfo
    , const QString &password
    , const QString &systemName
    , const OperationCallback &callback)
{
    if (systemName.isEmpty())
    {
        if (callback)
            callback(RequestError::kUnspecified, Constants::kSystemNameAffected);
        return;
    }

    static const QString kSystemNameTag = "systemName";
    static const QString oldPasswordTag = "oldPassword";

    QUrlQuery query;
    query.addQueryItem(kSystemNameTag, systemName);
    query.addQueryItem(oldPasswordTag, password);

    const RestClient::Request request(baseInfo
        , password, kConfigureCommand, query, RestClient::kUseStandardTimeout
        , makeSuccessCallback(callback, Constants::kSystemNameAffected)
        , makeErrorCallback(callback, Constants::kSystemNameAffected)); 
    RestClient::sendGet(request);
}

///

void rtu::sendSetPasswordRequest(const BaseServerInfoPtr &baseInfo
    , const QString &currentPassword
    , const QString &password
    , bool useNewPassword
    , const OperationCallback &callback)
{
    if (password.isEmpty())
    {
        if (callback)
            callback(RequestError::kUnspecified, Constants::kPasswordAffected);

        return;
    }

    static const QString newPasswordTag = "password";
    static const QString oldPasswordTag = "oldPassword";
    
    const QString authPass = (useNewPassword ? password : currentPassword);
    QUrlQuery query;
    query.addQueryItem(newPasswordTag, password);
    query.addQueryItem(oldPasswordTag, authPass);
    
    const RestClient::Request request(baseInfo
        , authPass, kConfigureCommand, query, RestClient::kUseStandardTimeout
        , makeSuccessCallback(callback, Constants::kPasswordAffected)
        , makeErrorCallback(callback, Constants::kPasswordAffected)); 
    RestClient::sendGet(request);
}

///

void rtu::sendSetPortRequest(const BaseServerInfoPtr &baseInfo
    , const QString &password
    , int port
    , const OperationCallback &callback)
{
    if (!port)
    {
        if (callback)
            callback(RequestError::kUnspecified, Constants::kPortAffected);
       return;
    }

    static const QString oldPasswordTag = "oldPassword";

    QUrlQuery query;
    query.addQueryItem(kPortTag, QString::number(port));
    query.addQueryItem(oldPasswordTag, password);

    const RestClient::Request request(baseInfo
        , password, kConfigureCommand, query, RestClient::kUseStandardTimeout
        , makeSuccessCallback(callback, Constants::kPortAffected)
        , makeErrorCallback(callback, Constants::kPortAffected)); 
    RestClient::sendGet(request);
}

void rtu::sendChangeItfRequest(const BaseServerInfoPtr &baseInfo
    , const QString &password
    , const ItfUpdateInfoContainer &updateInfos
    , const OperationCallback &callback)
{
    QJsonArray jsonInfoChanges;
    Constants::AffectedEntities affected = 0;
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
            affected |= rtu::Constants::kDHCPUsageAffected;
        }
        
        if (change.ip && !change.ip->isEmpty())
        {
            jsonInfoChange.insert(kIpTag, *change.ip);
            affected |= rtu::Constants::kIpAddressAffected;
        }
        
        if (change.mask && !change.mask->isEmpty())
        {
            jsonInfoChange.insert(kMaskTag, *change.mask);
            affected |= rtu::Constants::kMaskAffected;
        }
        
        if (change.dns)
        {
            jsonInfoChange.insert(kDnsTag, *change.dns);
            affected |= rtu::Constants::kDNSAffected;
        }
        
        if (change.gateway)
        {
            jsonInfoChange.insert(kGatewayTag, *change.gateway);
            affected |= rtu::Constants::kGatewayAffected;
        }
        
        jsonInfoChanges.append(jsonInfoChange);   
    }

    const RestClient::Request request(baseInfo, password, kIfConfigCommand, QUrlQuery()
        , RestClient::kUseStandardTimeout, makeSuccessCallback(callback, affected)
        , makeErrorCallback(callback, affected));
    RestClient::sendPost(request, QJsonDocument(jsonInfoChanges).toJson());
}

///

rtu::ItfUpdateInfo::ItfUpdateInfo()
    : name()
    , useDHCP()
    , ip()
    , mask()
    , dns()
    , gateway()
{
}

rtu::ItfUpdateInfo::ItfUpdateInfo(const ItfUpdateInfo &other)
    : name(other.name)
    , useDHCP(other.useDHCP ? new bool (*other.useDHCP) : nullptr)
    , ip(other.ip ? new QString(*other.ip) : nullptr)
    , mask(other.mask ? new QString(*other.mask) : nullptr)
    , dns(other.dns ? new QString(*other.dns) : nullptr)
    , gateway(other.gateway ? new QString(*other.gateway) : nullptr)
{
}

rtu::ItfUpdateInfo::ItfUpdateInfo(const QString &initName)
    : name(initName)
    , useDHCP()
    , ip()
    , mask()
    , dns()
    , gateway()    
{
}

rtu::ItfUpdateInfo &rtu::ItfUpdateInfo::operator =(const ItfUpdateInfo &other)
{
    name = other.name;
    useDHCP = BoolPointer(other.useDHCP ? new bool (*other.useDHCP) : nullptr);
    ip = StringPointer(other.ip ? new QString(*other.ip) : nullptr);
    mask = StringPointer(other.mask ? new QString(*other.mask) : nullptr);
    dns = StringPointer(other.dns ? new QString(*other.dns) : nullptr);
    gateway = StringPointer(other.gateway ? new QString(*other.gateway) : nullptr);
    return *this;
}

