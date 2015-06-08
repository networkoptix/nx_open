
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
        , const QString &namespaceTag)
    {
        QUrl url;
        url.setScheme("http");
        url.setHost(host);
        url.setPort(port);
        url.setUserName(user);
        url.setPassword(password);
        url.setPath(namespaceTag);
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
        
        const rtu::InterfaceInfo ifInfo = { ifObject.value(kNameTag).toString()
            , ifObject.value(kIpAddressTag).toString(), ifObject.value(kMacAddressTag).toString()
            , ifObject.value(kMaskTag).toString(), ifObject.value(kGateWayTag).toString()
            , ifObject.value(kDHCPTag).toBool() ? Qt::Checked : Qt::Unchecked };
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

    const QDateTime &utcTime = 
        QDateTime::fromMSecsSinceEpoch(timeData.value(kUtcTimeTag).toInt());
    
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
    const auto successfulCallback = 
        [successful, failed, id, password](const QByteArray &data)
    {
        rtu::ExtraServerInfo extraInfo;
        const bool parsed = parseExtraInfoCommand(kGetServerExtraInfoCmd
            , QJsonDocument::fromJson(data.data()).object(), extraInfo);
                
        if (parsed)
        {
            if (successful)
                successful(id, extraInfo);
        }
        else if (failed)
        {
            failed(id);
        }
    };

    const auto failedCallback = [failed, id](const QString &) 
    {
        if (failed)
            failed(id);
    };
    
    client->sendGet(url, successfulCallback, failed);
}

rtu::HttpClient::OnErrorReplyFunc makeFailedCallback(const rtu::OperationCallback &callback
    , rtu::AffectedEntities affected)
{
    const auto result = [callback, affected](const QString &reason)
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
    
    const auto fCallback = [failedCallback, id] (const QString &) 
    {
        if (failedCallback)
            failedCallback(id); 
    };
    
    client->sendGet(url, sCallback, fCallback);
    return true;
}

bool rtu::setTimeRequest(HttpClient *client
    , const ServerInfo &info
    , const QDateTime &dateTime
    , const DateTimeCallbackType &succesfulCallback
    , const FailedCallback &failedCallback)
{
    return true;
}

QJsonObject serializeAddr(const rtu::InterfaceInfo &address
    , rtu::AffectedEntities &affected)
{
    static const QString kIpTag = "ipAddr";
    static const QString kMaskTag = "netMask";
    static const QString kDHCPFlagTag = "dhcp";
    static const QString kMacAddressTag = "mac";
    
    QJsonObject jsonAddr;
    if (!address.macAddress.isEmpty())
        jsonAddr.insert(kMacAddressTag, address.macAddress);
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

bool rtu::changeIpsRequest(HttpClient *client
    , const ServerInfo &info
    , const InterfaceInfoList &addresses
    , const OperationCallback &callback)
{
    const QString kCommandName = "changeIpAddresses";

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
        callback(QString(), 0);
    };
    
    const auto &failedCallback = [callback](int errorCode) 
    {
        callback(QString("No error"), 0);
    };

    qDebug() << jsonAddresses;
    client->sendPost(url, QJsonDocument(jsonAddresses).toBinaryData(), successfullCallback
        , makeFailedCallback(callback, affected));
    return true;
}

