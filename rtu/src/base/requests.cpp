
#include "requests.h"

#include <QUrl>
#include <QUrlQuery>

#include <QDateTime>
#include <QTimeZone>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

#include <base/server_info.h>
#include <helpers/http_client.h>

#include <QDebug>

namespace 
{
    const QString kNoParamRequestTemplate = "http://%1:%2/%3/%4";
    const QString kParamsPartTemplate = "?%1";
    
    const QString kApiNamespaceTag = "/api/";

    const QString kConfigureCommand = kApiNamespaceTag + "configure";
    const QString kIfConfigCommand = kApiNamespaceTag + "ifconfig";
    const QString kSetTimeCommand = kApiNamespaceTag + "settime";
    const QString kGetTimeCommand = kApiNamespaceTag + "gettime";

    QUrl makeUrl(const QString &host
        , const int port
        , const QString &password
        , const QString &command)

    {
        qDebug() << "---- " << command << ":" << password;
        
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
                qDebug() << data;
                callback(getErrorDescription(object), affected);
            }
            else
            {
                callback(QString(), rtu::kNoEntitiesAffected);
            }
        };
        
        return result;
    }
    
    rtu::HttpClient::ErrorCallback makeErrorCallback(const rtu::OperationCallback &callback
        , rtu::AffectedEntities affected)
    {
        const auto result = [callback, affected](const QString &reason, int)
        {
            qDebug() << "\t\t\t" << reason;
            callback(reason, affected);
        };
        
        return result;
    }
}

const QString &rtu::adminUserName()
{
    static QString result("admin");
    return result;
}

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

        const QJsonObject &ifObject = interface.toObject();
        if (!ifObject.contains(kDHCPTag) || !ifObject.contains(kGateWayTag)
            || !ifObject.contains(kIpAddressTag) || !ifObject.contains(kMacAddressTag)
            || !ifObject.contains(kNameTag) || !ifObject.contains(kMaskTag))
        {
            return false;
        }
        
        const rtu::InterfaceInfo &ifInfo = rtu::InterfaceInfo(
            ifObject.value(kNameTag).toString()
            , ifObject.value(kIpAddressTag).toString()
            , ifObject.value(kMacAddressTag).toString()
            , ifObject.value(kMaskTag).toString()
            , ifObject.value(kGateWayTag).toString()
            , ifObject.value(kDHCPTag).toBool() ? Qt::Checked : Qt::Unchecked);
        interfaces.push_back(ifInfo);
    }
    
    extraInfo.interfaces = interfaces;
    return true;
}

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
    
    const QTimeZone &timeZone = (containsTimeZoneId 
        ? QTimeZone(timeData.value(kTimeZoneIdTag).toString().toStdString().c_str())
        : QTimeZone(timeData.value(kTimeZoneOffsetTag).toInt()));

    const qint64 msecs = timeData.value(kUtcTimeTag).toString().toLongLong();
    
    extraInfo.timeZone = timeZone;
    extraInfo.utcDateTime = QDateTime::fromMSecsSinceEpoch(msecs, Qt::UTC);

    extraInfo.timestamp = QDateTime::currentDateTime();

    return true;    
}

/// Forward declarations
bool parseExtraInfoCommand(const QString &cmdName
    , const QJsonObject &object
    , rtu::ExtraServerInfo &extraInfo);

static const QString kGetTimeCmdName = kApiNamespaceTag + "gettime";
static const QString kIfListCmdName = kApiNamespaceTag + "iflist";
static const QString kGetServerExtraInfoCmd = kApiNamespaceTag + "aggregator";

bool parseGetServerExtraInfoCmd(const QJsonObject &object
    , rtu::ExtraServerInfo &extraInfo)
{
    if (isErrorReply(object))
        return false;
    
    const QJsonObject &body = extractReplyBody(object).toObject();
    if (body.isEmpty())
        return false;
    
    if (!body.contains(kGetTimeCmdName))
        return false;
    
    if (!parseExtraInfoCommand(kGetTimeCmdName
        , body.value(kGetTimeCmdName).toObject(), extraInfo))
    {
        return false;
    }
    
    if (body.contains(kIfListCmdName))
        parseIfListCmd(body.value(kIfListCmdName).toObject(), extraInfo);
    
    return true;
}

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
        result.insert(kGetTimeCmdName, [](const QJsonObject &object
            , rtu::ExtraServerInfo &extraInfo) { return parseGetTimeCmd(object, extraInfo); });            
        result.insert(kIfListCmdName, [](const QJsonObject &object
            , rtu::ExtraServerInfo &extraInfo) { return parseIfListCmd(object, extraInfo); });            
        result.insert(kGetServerExtraInfoCmd, [](const QJsonObject &object
            , rtu::ExtraServerInfo &extraInfo) { return parseGetServerExtraInfoCmd(object, extraInfo); });            
        return result;
    }();
    
    const auto &itParser = parsers.find(cmdName);
    if (itParser == parsers.end())
        return false;
    
    return itParser.value()(object, extraInfo);
}

void rtu::getServerExtraInfo(HttpClient *client
    , const BaseServerInfo &baseInfo
    , const QString &password
    , const ExtraServerInfoSuccessCallback &successful
    , const FailedCallback &failed)
{
    Q_ASSERT(client);
    
    static const QString kCmdTag = "exec_cmd";
    static const QString kLocalTimeFlagTag = "local";

    QUrlQuery query;
    query.addQueryItem(kCmdTag, kGetTimeCmdName);
    query.addQueryItem(kLocalTimeFlagTag, QString());
    query.addQueryItem(kCmdTag, kIfListCmdName);
    
    QUrl url = makeUrl(baseInfo.hostAddress, baseInfo.port
        , password, kGetServerExtraInfoCmd);
    url.setQuery(query);

    const QUuid &id = baseInfo.id;
    
    const auto handleSuccessfullResult = 
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
            failed(id);
        }
    };

    const auto successfulCallback = [handleSuccessfullResult](const QByteArray &data)
    {
        handleSuccessfullResult(data, kGetServerExtraInfoCmd);
    };

    const auto failedCallback = 
        [client, baseInfo, failed, password, handleSuccessfullResult](const QString &r, int)
    {
        qDebug() << r;
        const auto succesfulCallback = 
            [handleSuccessfullResult](const QByteArray &data)
        {
            handleSuccessfullResult(data, kGetTimeCmdName);
        };
        
        const auto failedCallback = 
            [failed, baseInfo](const QString &, int)
        {
            if (failed)
                failed(baseInfo.id);
        };

        const QUrl url = makeUrl(baseInfo.hostAddress, baseInfo.port
            , password, kGetTimeCmdName);
        client->sendGet(url, succesfulCallback, failedCallback);
    };
    
    qDebug() << url;
    qDebug() << password;
    client->sendGet(url, successfulCallback, failedCallback);
}

const QString kReplyTag = "reply";

bool rtu::getTimeRequest(HttpClient *client
    , const ServerInfo &info
    , const DateTimeCallbackType &successfullCallback
    , const FailedCallback &failedCallback)
{
    const QUrl url = makeUrl(info, kGetTimeCommand);
    
    const QUuid id = info.baseInfo().id;
    const auto sCallback = [id, successfullCallback, failedCallback](const QByteArray &data)
    {
        const QJsonObject object = QJsonDocument::fromJson(data.data()).object();
        const auto itReply = object.find(kReplyTag);
        if (itReply == object.end())
        {
            failedCallback(id);
            return;
        }
        
        static const QString kUtcTimeTag = "utcTime";
        static const QString kTimeZoneOffsetTag = "timezoneId";
        
        const auto &reply = itReply.value().toObject();
        const auto &itTimeZone = reply.find(kTimeZoneOffsetTag);
        const auto &itUtcTime = reply.find(kUtcTimeTag);
        if ((itTimeZone == reply.end()) || (itUtcTime == reply.end()))
        {
            failedCallback(id);
            return;
        }
        
        const int msecsFromUtc = itUtcTime.value().toInt();
        const QTimeZone &timeZone = QTimeZone(QByteArray(itTimeZone.value().toString().toStdString().c_str()));
        const QDateTime &utcServerTime = QDateTime::fromMSecsSinceEpoch(msecsFromUtc, Qt::UTC);
        successfullCallback(id, utcServerTime, timeZone, QDateTime::currentDateTimeUtc());
    };
    
    const auto fCallback = [failedCallback, id] (const QString &, int) 
    {
        if (failedCallback)
            failedCallback(id); 
    };
    
    client->sendGet(url, sCallback, fCallback);
    return true;
}

#include <helpers/time_helper.h>

bool rtu::sendSetTimeRequest(HttpClient *client
    , const ServerInfo &info
    , const QDateTime &utcDateTime
    , const QTimeZone &timeZone
    , const OperationCallback &callback)
{
    if (!info.hasExtraInfo() || !utcDateTime.isValid() 
        || !timeZone.isValid() || !client)
    {
        return false;
    }
    
    static const QString &kDateTimeTag = "datetime";
    static const QString &kTimeZoneTag = "timezone";
    
    QUrl url = makeUrl(info, kSetTimeCommand);
    
    QUrlQuery query;
    query.addQueryItem(kDateTimeTag, QString::number(utcDateTime.toMSecsSinceEpoch()));
    query.addQueryItem(kTimeZoneTag, timeZone.id());
    url.setQuery(query);
    
    const AffectedEntities affected = (kDateTimeAffected | kTimeZoneAffected);
    client->sendGet(url, makeReplyCallback(callback, affected)
        , makeErrorCallback(callback, affected));
    return true;
}

///

bool rtu::sendSetSystemNameRequest(HttpClient *client
    , const ServerInfo &info
    , const QString &systemName
    , const OperationCallback &callback)
{
    if (!client || !info.hasExtraInfo() || systemName.isEmpty())
        return false;

    static const QString kSystemNameTag = "systemName";
    static const QString oldPasswordTag = "oldPassword";

    QUrlQuery query;
    query.addQueryItem(kSystemNameTag, systemName);
    query.addQueryItem(oldPasswordTag, info.extraInfo().password);

    QUrl url = makeUrl(info, kConfigureCommand);
    url.setQuery(query);

    qDebug() << "___" << url << " : " << info.extraInfo().password;
    client->sendGet(url, makeReplyCallback(callback, kSystemNameAffected)
        , makeErrorCallback(callback, kSystemNameAffected));
    return true;
}

bool rtu::sendSetPasswordRequest(HttpClient *client
    , const ServerInfo &info
    , const QString &password
    , bool useNewPassword
    , const OperationCallback &callback)
{
    if (!client || !info.hasExtraInfo() || password.isEmpty())
        return false;

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
    return true;    
}

bool rtu::sendSetPortRequest(HttpClient *client
    , const ServerInfo &info
    , int port
    , const OperationCallback &callback)
{
    if (!client || !info.hasExtraInfo() || !port)
        return false;

    static const QString kPortTag = "port";
    static const QString oldPasswordTag = "oldPassword";

    QUrlQuery query;
    query.addQueryItem(kPortTag, QString::number(port));
    query.addQueryItem(oldPasswordTag, info.extraInfo().password);

    QUrl url = makeUrl(info, kConfigureCommand);
    url.setQuery(query);

    client->sendGet(url, makeReplyCallback(callback, kPortAffected)
        , makeErrorCallback(callback, kPortAffected));
    return true;
}


void rtu::interfacesListRequest(HttpClient *client
    , const BaseServerInfo &info
    , const QString &password
    , const ExtraServerInfoSuccessCallback &successful
    , const FailedCallback &failed)
{
    const QUrl url = makeUrl(info.hostAddress, info.port
        , password, kIfListCmdName);

    const auto id = info.id;
    const auto &successfullCallback =
        [id, successful, failed](const QByteArray &data)
    {
        const QJsonObject &object = QJsonDocument::fromJson(data.data()).object();

        rtu::ExtraServerInfo extraInfo;
        if (!parseIfListCmd(object, extraInfo))
        {
            if (failed)
                failed(id);
        }
        else if (successful)
        {
            successful(id, extraInfo);
        }
    };

    const auto &failedCallback = [id, failed](const QString &reason, int code)
    {
        if (failed)
            failed(id);
    };

    qDebug() << "++++++ QUrl: " << url;
    client->sendGet(url, successfullCallback, failedCallback);
}

rtu::ItfUpdateInfo::ItfUpdateInfo()
    : name()
    , useDHCP(true)
    , ip()
    , mask()
{
}

rtu::ItfUpdateInfo::ItfUpdateInfo(const ItfUpdateInfo &other)
    : name(other.name)
    , useDHCP(other.useDHCP)
    , ip(other.ip ? new QString(*other.ip) : nullptr)
    , mask(other.mask ? new QString(*other.mask) : nullptr)
{
}

rtu::ItfUpdateInfo::ItfUpdateInfo(const QString &initName
    , bool initUseDHCP
    , const StringPointer &initIp
    , const StringPointer &initMask)
    : name(initName)
    , useDHCP(initUseDHCP)
    , ip(initIp ? new QString(*initIp) : nullptr)
    , mask(initMask ? new QString(*initMask) : nullptr)
{
}

rtu::ItfUpdateInfo &rtu::ItfUpdateInfo::operator =(const ItfUpdateInfo &other)
{
    ItfUpdateInfo tmp(other);
    std::swap(tmp, *this);
    return *this;
}

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
        
        QJsonObject jsonInfoChange;
        if (!change.name.isEmpty())
            jsonInfoChange.insert(kNameTag, change.name);
        
        jsonInfoChange.insert(kDHCPFlagTag, change.useDHCP);
        affected |= rtu::kDHCPUsageAffected;
        
        if (!change.ip && !change.ip->isEmpty())
        {
            jsonInfoChange.insert(kIpTag, *change.ip);
            affected |= rtu::kIpAddressAffected;
        }
        
        if (!change.mask && !change.mask->isEmpty())
        {
            jsonInfoChange.insert(kMaskTag, *change.mask);
            affected |= rtu::kMaskAffected;
        }
        
        jsonInfoChanges.append(jsonInfoChange);   
    }

    QUrl url = makeUrl(info, kIfConfigCommand);
    client->sendPost(url, QJsonDocument(jsonInfoChanges).toJson()
        , makeReplyCallback(callback, affected)
        , makeErrorCallback(callback, affected));
}

