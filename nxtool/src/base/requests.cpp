
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

    const QString kConfigureCommand = kApiNamespaceTag + "configure";
    const QString kIfConfigCommand = kApiNamespaceTag + "ifconfig";
    const QString kSetTimeCommand = kApiNamespaceTag + "settime";
    const QString kGetTimeCommand = kApiNamespaceTag + "gettime";
    const QString kIfListCommand = kApiNamespaceTag + "iflist";
    const QString kGetServerExtraInfoCommand = kApiNamespaceTag + "aggregator";

    const QString &kInvalidRequest = "Invalid request parameters";
    const QString &kErrorDesc = "Invalid response";

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
    
    QJsonValue extractReplyBody(const QJsonObject &object)
    {
        static const QString &kReplyTag = "reply";
        return (object.contains(kReplyTag) ? object.value(kReplyTag) : QJsonValue());
    }
    
    bool isErrorReply(const QJsonObject &object)
    {
        static const QString &kErrorTag = "error";
        const int code = (!object.contains(kErrorTag) ? 0
            : object.value(kErrorTag).toString().toInt());
        return code;
    }
    
    QString getErrorDescription(const QJsonObject &object)
    {
        static const QString &kReasonTag = "errorString";
        return (object.contains(kReasonTag) ? 
            object.value(kReasonTag).toString() : QString());
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
                callback(getErrorDescription(object), affected);
            }
            else
            {
                //TODO: #ynikitenkov #High Describe why no entities are affected here?
                callback(QString(), affected /*rtu::kNoEntitiesAffected*/ );
            }
        };
        
        return result;
    }
    
    rtu::HttpClient::ErrorCallback makeErrorCallback(const rtu::OperationCallback &callback
        , rtu::AffectedEntities affected)
    {
        const auto result = [callback, affected](const QString &reason, int)
        {
            if (!callback)
                return;
            
            callback(reason, affected);
        };
        
        return result;
    }
}

/// Forward declarations
bool parseExtraInfoCommand(const QString &cmdName
    , const QJsonObject &object
    , rtu::ExtraServerInfo &extraInfo);

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

bool parseGetServerExtraInfoCmd(const QJsonObject &object
    , rtu::ExtraServerInfo &extraInfo)
{
    if (isErrorReply(object))
        return false;
    
    const QJsonObject &body = extractReplyBody(object).toObject();
    if (body.isEmpty())
        return false;
    
    if (!body.contains(kGetTimeCommand))
        return false;
    
    if (!parseExtraInfoCommand(kGetTimeCommand
        , body.value(kGetTimeCommand).toObject(), extraInfo))
    {
        return false;
    }
    
    if (body.contains(kIfListCommand))
        parseIfListCmd(body.value(kIfListCommand).toObject(), extraInfo);
    
    return true;
}

///

bool parseExtraInfoCommand(const QString &cmdName
    , const QJsonObject &object
    , rtu::ExtraServerInfo &extraInfo)
{
    typedef std::function<bool (const QJsonObject &object
        , rtu::ExtraServerInfo &extraInfo)> ParseFunction;
    
    typedef QHash<QString, ParseFunction> Parsers;
    
    static const Parsers parsers = []() -> Parsers 
    {
        Parsers result;
        result.insert(kGetTimeCommand, [](const QJsonObject &object
            , rtu::ExtraServerInfo &extraInfo) { return parseGetTimeCmd(object, extraInfo); });            
        result.insert(kIfListCommand, [](const QJsonObject &object
            , rtu::ExtraServerInfo &extraInfo) { return parseIfListCmd(object, extraInfo); });            
        result.insert(kGetServerExtraInfoCommand, [](const QJsonObject &object
            , rtu::ExtraServerInfo &extraInfo) { return parseGetServerExtraInfoCmd(object, extraInfo); });            
        return result;
    }();
    
    const auto &itParser = parsers.find(cmdName);
    if (itParser == parsers.end())
        return false;
    
    return itParser.value()(object, extraInfo);
}

///

void rtu::getServerExtraInfo(HttpClient *client
    , const BaseServerInfo &baseInfo
    , const QString &password
    , const ExtraServerInfoSuccessCallback &successful
    , const OperationCallback &failed)
{
    if (!client)
    {
        if (failed)
            failed(kInvalidRequest, kAllEntitiesAffected);
        return;
    }

    static const QString kCmdTag = "exec_cmd";
    static const QString kLocalTimeFlagTag = "local";

    QUrlQuery query;
    query.addQueryItem(kCmdTag, kGetTimeCommand);
    query.addQueryItem(kLocalTimeFlagTag, QString());
    query.addQueryItem(kCmdTag, kIfListCommand);
    
    QUrl url = makeUrl(baseInfo.hostAddress, baseInfo.port
        , password, kGetServerExtraInfoCommand);
    url.setQuery(query);

    const QUuid &id = baseInfo.id;
    
    const auto &handleSuccessfullResult = 
        [id, successful, failed, password](const QByteArray &data, const QString &cmdName)
    {
        rtu::ExtraServerInfo extraInfo;
        const bool parsed = parseExtraInfoCommand(cmdName
            , QJsonDocument::fromJson(data.data()).object(), extraInfo);
                
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
            failed(kErrorDesc, kAllAddressFlagsAffected 
                | kTimeZoneAffected | kDateTimeAffected);
        }
    };

    const auto &successfulCallback = [handleSuccessfullResult](const QByteArray &data)
    {
        handleSuccessfullResult(data, kGetServerExtraInfoCommand);
    };

    const auto failedCallback = 
        [client, baseInfo, failed, password, handleSuccessfullResult](const QString &, int)
    {
        const auto &succesful = 
            [handleSuccessfullResult](const QByteArray &data)
        {
            handleSuccessfullResult(data, kGetTimeCommand);
        };

        const QUrl url = makeUrl(baseInfo.hostAddress, baseInfo.port
            , password, kGetTimeCommand);
        client->sendGet(url, succesful
            , makeErrorCallback(failed, kAllEntitiesAffected));
    };
    
    client->sendGet(url, successfulCallback, failedCallback);
}

///

void rtu::sendIfListRequest(HttpClient *client
    , const BaseServerInfo &info
    , const QString &password
    , const ExtraServerInfoSuccessCallback &successful
    , const OperationCallback &failed)
{
    if (!client)
    {
        if (failed)
            failed(kInvalidRequest, kAllEntitiesAffected);
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
                failed(kErrorDesc, kAllAddressFlagsAffected);
        }
        else if (successful)
        {
            successful(id, extraInfo);
        }
    };

   client->sendGet(url, successfullCallback
        , makeErrorCallback(failed, kAllAddressFlagsAffected));
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
            callback(kInvalidRequest, kDateTimeAffected | kTimeZoneAffected);
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
            callback(kInvalidRequest, kSystemNameAffected);
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
            callback(kInvalidRequest, kPasswordAffected);

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
            callback(kInvalidRequest, kPortAffected);
       return;
    }

    static const QString kPortTag = "port";
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
            callback(kInvalidRequest, affected);
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

