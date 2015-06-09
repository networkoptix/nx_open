
#include "requests.h"

#include <QUrl>
#include <QUrlQuery>

#include <QDateTime>
#include <QTimeZone>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

#include <server_info.h>
#include <helpers/http_client.h>

#include <QDebug>

namespace 
{
    const QString kNoParamRequestTemplate = "http://%1:%2/%3/%4";
    const QString kParamsPartTemplate = "?%1";
    
    const QString kApiNamespaceTag = "/api/";
    const QString kEc2NamespaceTag = "/ec2/";   
    
    QUrl makeUrl(const QString &host
        , const int port
        , const QString &user
        , const QString &password
        , const QString &kCommand)
    {
        QUrl url;
        url.setScheme("http");
        url.setHost(host);
        url.setPort(port);
        url.setUserName(user);
        url.setPassword(password);
        url.setPath(kCommand);
        return url;
    }
    
    
}

const QString &rtu::adminUserName()
{
    static QString result("admin");
    return result;
}

const QString &rtu::defaultAdminPassword()
{
    static QString result("123");
    return result;
}

QJsonValue extractReplyBody(const QJsonObject &object)
{
    static const QString &kReplyTag = "reply";
    return (object.contains(kReplyTag) ? object.value(kReplyTag) : QJsonValue());
}

bool isErrorReply(const QJsonObject &object)
{
    static const QString &kErrorTag = "error";
    return (object.contains(kErrorTag) ? object.value(kErrorTag).toInt() : true);
}

QString getErrorDescription(const QJsonObject &object)
{
    static const QString &kReasonTag = "errorString";
    return (object.contains(kReasonTag) ? 
        object.value(kReasonTag).toString() : QString());
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
    const QDateTime &utcTime = QDateTime::fromMSecsSinceEpoch(msecs);
    
    extraInfo.dateTime = (timeZone.isValid() ? utcTime.toTimeZone(timeZone) : utcTime);
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
    
    QUrlQuery query;
    query.addQueryItem(kCmdTag, kGetTimeCmdName);
    query.addQueryItem(kCmdTag, kIfListCmdName);
    
    QUrl url = makeUrl(baseInfo.hostAddress, baseInfo.port
        , adminUserName(), password, kGetServerExtraInfoCmd);
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
        [client, baseInfo, failed, password, handleSuccessfullResult](const QString &, int) 
    {
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
            , adminUserName(), password, kGetTimeCmdName);
        client->sendGet(url, succesfulCallback, failedCallback);
    };
    
    client->sendGet(url, successfulCallback, failedCallback);
}

rtu::HttpClient::OnErrorReplyFunc makeFailedCallback(const rtu::OperationCallback &callback
    , rtu::AffectedEntities affected)
{
    const auto result = [callback, affected](const QString &reason, int)
    {
        callback(reason, affected);
    };
    
    return result;
}

bool rtu::configureRequest(HttpClient *client
    , const OperationCallback &callback
    , const ServerInfo &info
    , const QString &systemName
    , const QString &password
    , const int newPort)
{
    if (!client || (systemName.isEmpty() && password.isEmpty() && !newPort))// && !newPort))
        return false;
    
    static const QString kSystemNameParamName = "systemName";
    static const QString kPortParamName = "port";
    static const QString kNewPasswordParamName = "password";
    static const QString kCurrentPasswordParamName = "oldPassword";
    
    static const QString kCommandName = "configure";
    
    QUrl url = makeUrl(info.baseInfo().hostAddress, info.baseInfo().port
        , adminUserName(), info.extraInfo().password
        , kApiNamespaceTag + kCommandName);
    
    QUrlQuery query;
    AffectedEntities affected = 0;
    if (!systemName.isEmpty())
    {
        query.addQueryItem(kSystemNameParamName, systemName);
        affected |= kSystemName;
    }

    if (!password.isEmpty())
    {
        query.addQueryItem(kNewPasswordParamName, password);
        affected |= kPassword;
    }
    
    if (newPort)
    {
        query.addQueryItem(kPortParamName, QString::number(newPort));
        affected |= kPort;
    }
    
    if (!password.isEmpty() || newPort)
        query.addQueryItem(kCurrentPasswordParamName, info.extraInfo().password);
    
    url.setQuery(query);
    
    const auto successfullCallback = [callback](const QByteArray &data)
    {
        qDebug() << data;
        callback(QString(), 0);
    };
    
    qDebug() << url;
    client->sendGet(url, successfullCallback, makeFailedCallback(callback, affected));
    return true;
}

const QString kReplyTag = "reply";


bool rtu::getTimeRequest(HttpClient *client
    , const ServerInfo &info
    , const DateTimeCallbackType &successfullCallback
    , const FailedCallback &failedCallback)
{
    static const QString kCommandName = "gettime";
    
    const QUrl url = makeUrl(info.baseInfo().hostAddress, info.baseInfo().port, adminUserName()
        , info.extraInfo().password, kApiNamespaceTag + kCommandName);
    
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
        
        const QDateTime &currentTime = QDateTime::currentDateTimeUtc();     

        const int msecsFromUtc = itUtcTime.value().toInt();
        const QTimeZone timeZone = QTimeZone(QByteArray(itTimeZone.value().toString().toStdString().c_str()));
        
        const QDateTime &utcServerTime = QDateTime::fromMSecsSinceEpoch(msecsFromUtc, Qt::UTC);
        const QDateTime &serverTime = QDateTime::fromMSecsSinceEpoch(msecsFromUtc, timeZone);
        successfullCallback(id, serverTime, QDateTime::currentDateTimeUtc());
    };
    
    const auto fCallback = [failedCallback, id] (const QString &, int) 
    {
        if (failedCallback)
            failedCallback(id); 
    };
    
    client->sendGet(url, sCallback, fCallback);
    return true;
}

void rtu::setTimeRequest(HttpClient *client
    , const ServerInfo &info
    , const QDateTime &dateTime
    , const DateTimeCallbackType &successfulCallback
    , const OperationCallback &failedCallback)
{
    static const QString &kCmdName = kApiNamespaceTag + "settime";
    static const QString &kDateTimeTag = "datetime";
    static const QString &kTimeZoneTag = "timezone";
    
    if (!info.hasExtraInfo())
        return;
    
    QUrl url = makeUrl(info.baseInfo().hostAddress, info.baseInfo().port
        , adminUserName(), info.extraInfo().password, kCmdName);

    QString strTime = dateTime.toUTC().toString(Qt::ISODate);
    strTime.remove(strTime.size() - 1, 1);      /// Removes 'Z' character
    
    QUrlQuery query;
    query.addQueryItem(kDateTimeTag, strTime);
    query.addQueryItem(kTimeZoneTag, dateTime.timeZone().id());
    url.setQuery(query);
    
    const QDateTime timestamp = QDateTime::currentDateTime();
    const auto &successful = 
        [info, dateTime, timestamp, successfulCallback, failedCallback](const QByteArray &data)
    {
        const QJsonObject &object = QJsonDocument::fromJson(data.data()).object();
        if (isErrorReply(object))
        {
            if (failedCallback)
                failedCallback(getErrorDescription(object), kDateTime | kTimeZone);
        }
        else if (successfulCallback)
        {
            successfulCallback(info.baseInfo().id, dateTime, timestamp);
        }
    };

    const auto &failed = [failedCallback](const QString &errorReason, int)
    {
        if (failedCallback)
            failedCallback(errorReason, kDateTime | kTimeZone);
    };
    
    qDebug() << url;
    client->sendGet(url, successful, failed);
}

QJsonObject serializeAddr(const rtu::InterfaceInfo &address
    , rtu::AffectedEntities &affected)
{
    static const QString kIpTag = "ipAddr";
    static const QString kMaskTag = "netMask";
    static const QString kDHCPFlagTag = "dhcp";
    static const QString kNameTag = "name";
    
    QJsonObject jsonAddr;
    if (!address.name.isEmpty())
        jsonAddr.insert(kNameTag, address.name);
    
    if (!address.ip.isEmpty())
    {
        jsonAddr.insert(kIpTag, address.ip);
        affected |= rtu::kIpAddress;
    }
    if (!address.mask.isEmpty())
    {
        jsonAddr.insert(kMaskTag, address.mask);
        affected |= rtu::kSubnetMask;
    }

    jsonAddr.insert(kDHCPFlagTag, address.useDHCP != Qt::Unchecked);
    affected |= rtu::kDHCPUsage;
    
    return jsonAddr;
}

void rtu::changeIpsRequest(HttpClient *client
    , const ServerInfo &info
    , const InterfaceInfoList &addresses
    , const OperationCallback &callback)
{
    const QString kCommandName = "ifconfig";

    QJsonArray jsonAddresses;
    AffectedEntities affected = 0;
    for (const InterfaceInfo &address: addresses)
    {
        jsonAddresses.append(QJsonValue(serializeAddr(address, affected)));   
    }

    QUrl url = makeUrl(info.baseInfo().hostAddress, info.baseInfo().port
        , adminUserName(), info.extraInfo().password, kApiNamespaceTag + kCommandName);
    const auto &successfullCallback = [callback](const QByteArray &data) 
    {
        qDebug() <<"***" << data;
        callback(QString(), 0);
    };

    qDebug() << url;
    qDebug() << QJsonDocument(jsonAddresses).toJson();
    client->sendPost(url, QJsonDocument(jsonAddresses).toJson(), successfullCallback
        , makeFailedCallback(callback, affected));
}

