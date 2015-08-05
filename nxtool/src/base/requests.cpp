
#include "requests.h"

#include <QUrl>
#include <QUrlQuery>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

#include <base/server_info.h>
#include <helpers/http_client.h>
#include <helpers/time_helper.h>

#include <QDebug>

namespace 
{
    const QString kApiNamespaceTag = "/api/";

    const QString kModuleInformationCommand = kApiNamespaceTag + "moduleInformation";
    const QString kConfigureCommand = kApiNamespaceTag + "configure";
    const QString kIfConfigCommand = kApiNamespaceTag + "ifconfig";
    const QString kSetTimeCommand = kApiNamespaceTag + "settime";
    const QString kGetTimeCommand = kApiNamespaceTag + "gettime";
    const QString kIfListCommand = kApiNamespaceTag + "iflist";
    const QString kGetServerExtraInfoCommand = kApiNamespaceTag + "aggregator";

    const QString kInvalidRequest = "Invalid request parameters";
    const QString kErrorDesc = "Invalid response";

    const QString kIDTag = "id";
    const QString kSeedTag = "seed";
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
        url.setUserName(rtu::adminUserName());
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
        result.insert(kNameTag, [](const QJsonObject& object, rtu::BaseServerInfo &info)
            { info.name = object.value(kNameTag).toString(); });
        result.insert(kSystemNameTag, [](const QJsonObject& object, rtu::BaseServerInfo &info)
            { info.systemName = object.value(kSystemNameTag).toString(); });
        result.insert(kPortTag, [](const QJsonObject& object, rtu::BaseServerInfo &info)
            { info.port = object.value(kPortTag).toInt(); });


        result.insert(kFlagsTag, [](const QJsonObject& object, rtu::BaseServerInfo &info)
            {
                typedef QPair<QString, rtu::Constants::ServerFlag> TextFlagsInfo;
                static const TextFlagsInfo kKnownFlags[] =
                {
                    TextFlagsInfo("SF_timeCtrl", rtu::Constants::ServerFlag::AllowChangeDateTimeFlag)
                    , TextFlagsInfo("SF_IfListCtrl", rtu::Constants::ServerFlag::AllowIfConfigFlag)
                    , TextFlagsInfo("SF_AutoSystemName" , rtu::Constants::ServerFlag::IsFactoryFlag)
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
        const int code = (!object.contains(kErrorTag) ? 0
            : object.value(kErrorTag).toString().toInt());
        return code;
    }
   
    ///

    QString getErrorDescription(const QJsonObject &object)
    {
        static const QString &kReasonTag = "errorString";
        return (object.contains(kReasonTag) ? 
            object.value(kReasonTag).toString() : QString());
    }
    
    ///
    typedef std::function<bool (const QJsonObject &object
        , rtu::ExtraServerInfo &extraInfo)> ParseFunction;

    rtu::HttpClient::ReplyCallback makeReplyCallbackEx(
        const rtu::ExtraServerInfoSuccessCallback &successful
        , const rtu::OperationCallback &failed
        , const QUuid &id
        , const QString &password
        , rtu::AffectedEntities affected
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
                failed(rtu::kUnspecifiedError, kErrorDesc, affected);
            }
        };

        return result;
    }

    rtu::HttpClient::ReplyCallback makeReplyCallback(const rtu::OperationCallback &callback
        , rtu::AffectedEntities affected)
    {
        const auto &result = [callback, affected](const QByteArray &data)
        {
            if (!callback)
                return;
            
            const QJsonDocument &doc = QJsonDocument::fromJson(data.data());
            const QJsonObject &object = doc.object();
            if (isErrorReply(object))
            {
                callback(rtu::kUnspecifiedError, getErrorDescription(object), affected);
            }
            else
            {
                callback(rtu::kNoErrorReponse, QString(), affected);
            }
        };
        
        return result;
    }
    
    ///

    rtu::HttpClient::ErrorCallback makeErrorCallback(const rtu::OperationCallback &callback
        , rtu::AffectedEntities affected)
    {
        const auto result = [callback, affected](const QString &reason, int code)
        {
            if (!callback)
                return;
            
            callback(code, reason, affected);
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
}

///

const QString &rtu::adminUserName()
{
    static QString result("admin");
    return result;
}

///

const QStringList &rtu::defaultAdminPasswords()
{
    static QStringList result = []() -> QStringList
    {
        QStringList result;
        result.push_back("123");
        result.push_back("admin");
        return result;
    }();

    return result;
}

///

void rtu::parseModuleInformationReply(const QJsonObject &reply
    , rtu::BaseServerInfo &baseInfo)
{
    const QStringList &keys = reply.keys();
    for (const auto &key: keys)
    {
        const auto itHandler = parser.find(key);
        if (itHandler != parser.end())
            (*itHandler)(reply, baseInfo);
    }
}

///

void rtu::getTime(HttpClient *client
    , const BaseServerInfo &baseInfo
    , const QString &password
    , const ExtraServerInfoSuccessCallback &successful
    , const OperationCallback &failed
    , int timeout)
{
    static const AffectedEntities affected = (kTimeZoneAffected | kDateTimeAffected);

    if (!client)
    {
        if (failed)
            failed(rtu::kUnspecifiedError, kInvalidRequest, affected);
        return;
    }

    QUrl url = makeUrl(baseInfo.hostAddress, baseInfo.port
        , password, kGetTimeCommand);

    if (baseInfo.flags & Constants::AllowChangeDateTimeFlag)
    {
        QUrlQuery query;
        query.addQueryItem(kLocalTimeFlagTag, QString());
        url.setQuery(query);
    }

    const auto &parser = [](const QJsonObject &object, ExtraServerInfo &extraInfo)
    { 
        return parseGetTimeCmd(object, extraInfo);
    };
    const auto &localSuccessful = makeReplyCallbackEx(
        successful, failed, baseInfo.id, password, affected, parser);

    client->sendGet(url, localSuccessful
        , makeErrorCallback(failed, affected), timeout);
}

///

void rtu::getIfList(HttpClient *client
    , const BaseServerInfo &baseInfo
    , const QString &password
    , const ExtraServerInfoSuccessCallback &successful
    , const OperationCallback &failed
    , int timeout)
{
    static const AffectedEntities affected = kAllAddressFlagsAffected;
    if (!client)
    {
        if (failed)
            failed(kUnspecifiedError, kInvalidRequest, affected);
        return;
    }

    const auto &parser = [](const QJsonObject &object, ExtraServerInfo &extraInfo)
    { 
        return parseIfListCmd(object, extraInfo);
    };

    const auto &ifListSuccessfull = makeReplyCallbackEx(
        successful, failed, baseInfo.id, password, affected, parser);

    const QUrl url = makeUrl(baseInfo.hostAddress, baseInfo.port, password, kIfListCommand);
    client->sendGet(url, ifListSuccessfull, makeErrorCallback(failed, affected), timeout);
}

///

void rtu::getServerExtraInfo(HttpClient *client
    , const BaseServerInfo &baseInfo
    , const QString &password
    , const ExtraServerInfoSuccessCallback &successful
    , const OperationCallback &failed
    , int timeout)
{
    static const AffectedEntities affected = (kAllAddressFlagsAffected 
        | kTimeZoneAffected | kDateTimeAffected);

    if (!client)
    {
        if (failed)
            failed(kUnspecifiedError, kInvalidRequest, affected);
        return;
    }

    const auto &getTimeFailed = [failed](const int errorCode 
        , const QString &reason, AffectedEntities affected)
    {
        if (failed)
            failed(errorCode, reason, affected);
    };

    const auto &getTimeSuccessfull = [client, baseInfo, password, successful, timeout]
        (const QUuid &id, const rtu::ExtraServerInfo &extraInfo) 
    {
        /// getTime command is successfully executed up to now
        const auto &ifListFailed = [successful, id, extraInfo](const int, const QString &, AffectedEntities)
        {
            if (successful)
                successful(id, extraInfo);
        };

        const auto &ifListSuccessful = [successful, extraInfo](const QUuid &id, const rtu::ExtraServerInfo &ifListExtraInfo)
        {
            if (successful)
            {
                successful(id, ExtraServerInfo(extraInfo.password, extraInfo.timestampMs
                    , extraInfo.utcDateTimeMs, extraInfo.timeZoneId, ifListExtraInfo.interfaces));
            }
        };

        getIfList(client, baseInfo, password, ifListSuccessful, ifListFailed, timeout);
    };

    return getTime(client, baseInfo, password, getTimeSuccessfull, getTimeFailed, timeout);
}

///

void rtu::sendIfListRequest(HttpClient *client
    , const BaseServerInfo &info
    , const QString &password
    , const ExtraServerInfoSuccessCallback &successful
    , const OperationCallback &failed
    , int timeout)
{
    if (!client || !(info.flags & Constants::AllowIfConfigFlag))
    {
        if (failed)
            failed(kUnspecifiedError, kInvalidRequest, kAllEntitiesAffected);
        return;
    }
    
    const QUrl url = makeUrl(info.hostAddress, info.port
        , password, kIfListCommand);

    const auto id = info.id;
    const auto &successfullCallback =
        [id, successful, failed](const QByteArray &data)
    {
        const QJsonObject &object = QJsonDocument::fromJson(data.data()).object();

        rtu::ExtraServerInfo extraInfo;
        if (!parseIfListCmd(object, extraInfo))
        {
            if (failed)
                failed(kUnspecifiedError, kErrorDesc, kAllAddressFlagsAffected);
        }
        else if (successful)
        {
            successful(id, extraInfo);
        }
    };

   client->sendGet(url, successfullCallback
        , makeErrorCallback(failed, kAllAddressFlagsAffected), timeout);
}

///

void rtu::sendSetTimeRequest(HttpClient *client
    , const ServerInfo &info
    , qint64 utcDateTimeMs
    , const QByteArray &timeZoneId
    , const OperationCallback &callback)
{
    if (!info.hasExtraInfo() || utcDateTimeMs <= 0
        || !QTimeZone(timeZoneId).isValid() || !client)
    {
        if (callback)
            callback(kUnspecifiedError, kInvalidRequest, kDateTimeAffected | kTimeZoneAffected);
        return;
    }
    
    static const QString &kDateTimeTag = "datetime";
    static const QString &kTimeZoneTag = "timezone";
    
    QUrl url = makeUrl(info, kSetTimeCommand);
    
    QUrlQuery query;
    query.addQueryItem(kDateTimeTag, QString::number(utcDateTimeMs));
    query.addQueryItem(kTimeZoneTag, timeZoneId);
    url.setQuery(query);
    
    const AffectedEntities affected = (kDateTimeAffected | kTimeZoneAffected);
    
    client->sendGet(url, makeReplyCallback(callback, affected)
        , makeErrorCallback(callback, affected));
}

///

void rtu::sendSetSystemNameRequest(HttpClient *client
    , const ServerInfo &info
    , const QString &systemName
    , const OperationCallback &callback)
{
    if (!client || !info.hasExtraInfo() || systemName.isEmpty())
    {
        if (callback)
            callback(kUnspecifiedError, kInvalidRequest, kSystemNameAffected);
        return;
    }

    static const QString kSystemNameTag = "systemName";
    static const QString oldPasswordTag = "oldPassword";

    QUrlQuery query;
    query.addQueryItem(kSystemNameTag, systemName);
    query.addQueryItem(oldPasswordTag, info.extraInfo().password);

    QUrl url = makeUrl(info, kConfigureCommand);
    url.setQuery(query);

    client->sendGet(url, makeReplyCallback(callback, kSystemNameAffected)
        , makeErrorCallback(callback, kSystemNameAffected));
}

///

void rtu::sendSetPasswordRequest(HttpClient *client
    , const ServerInfo &info
    , const QString &password
    , bool useNewPassword
    , const OperationCallback &callback)
{
    if (!client || !info.hasExtraInfo() || password.isEmpty())
    {
        if (callback)
            callback(kUnspecifiedError, kInvalidRequest, kPasswordAffected);

        return;
    }

    static const QString newPasswordTag = "password";
    static const QString oldPasswordTag = "oldPassword";
    
    const QString authPass = (useNewPassword ? password : info.extraInfo().password);
    QUrlQuery query;
    query.addQueryItem(newPasswordTag, password);
    query.addQueryItem(oldPasswordTag, authPass);
    
    QUrl url = makeUrl(info.baseInfo().hostAddress, info.baseInfo().port, authPass, kConfigureCommand);
    url.setQuery(query);
    
    client->sendGet(url, makeReplyCallback(callback, kPasswordAffected)
        , makeErrorCallback(callback, kPasswordAffected));
}

///

void rtu::sendSetPortRequest(HttpClient *client
    , const ServerInfo &info
    , int port
    , const OperationCallback &callback)
{
    if (!client || !info.hasExtraInfo() || !port)
    {
        if (callback)
            callback(kUnspecifiedError, kInvalidRequest, kPortAffected);
       return;
    }

    static const QString oldPasswordTag = "oldPassword";

    QUrlQuery query;
    query.addQueryItem(kPortTag, QString::number(port));
    query.addQueryItem(oldPasswordTag, info.extraInfo().password);

    QUrl url = makeUrl(info, kConfigureCommand);
    url.setQuery(query);

    client->sendGet(url, makeReplyCallback(callback, kPortAffected)
        , makeErrorCallback(callback, kPortAffected));
}

///

void rtu::sendChangeItfRequest(HttpClient *client
    , const ServerInfo &info
    , const ItfUpdateInfoContainer &updateInfos
    , const OperationCallback &callback)
{
    QJsonArray jsonInfoChanges;
    AffectedEntities affected = 0;
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
            affected |= rtu::kDHCPUsageAffected;
        }
        
        if (change.ip && !change.ip->isEmpty())
        {
            jsonInfoChange.insert(kIpTag, *change.ip);
            affected |= rtu::kIpAddressAffected;
        }
        
        if (change.mask && !change.mask->isEmpty())
        {
            jsonInfoChange.insert(kMaskTag, *change.mask);
            affected |= rtu::kMaskAffected;
        }
        
        if (change.dns)
        {
            jsonInfoChange.insert(kDnsTag, *change.dns);
            affected |= rtu::kDNSAffected;
        }
        
        if (change.gateway)
        {
            jsonInfoChange.insert(kGatewayTag, *change.gateway);
            affected |= rtu::kGatewayAffected;
        }
        
        jsonInfoChanges.append(jsonInfoChange);   
    }

    if (!client || !info.hasExtraInfo())
    {
        if (callback)
            callback(kUnspecifiedError, kInvalidRequest, affected);
        return;
    }

    QUrl url = makeUrl(info, kIfConfigCommand);
    
    client->sendPost(url, QJsonDocument(jsonInfoChanges).toJson()
        , makeReplyCallback(callback, affected)
        , makeErrorCallback(callback, affected));
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

