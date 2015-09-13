
#include "requests.h"

#include <QUrl>
#include <QUrlQuery>

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
    const QString kGetServerExtraInfoCommand = kApiNamespaceTag + "aggregator";

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
   
    /*
    ///

    QString getErrorDescription(const QJsonObject &object)
    {
        static const QString &kReasonTag = "errorString";
        return (object.contains(kReasonTag) ? 
            object.value(kReasonTag).toString() : QString());
    }
    */
    ///
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
                failed(rtu::kUnspecifiedError, affected);
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
    rtu::RestClient::SuccessCallback makeSuccessCalback(const rtu::OperationCallback &callback
        , rtu::Constants::AffectedEntities affected)
    {
        return [callback, affected](const QByteArray & /* data */)
        {
            if (callback)
                callback(rtu::kNoErrorReponse, affected);
        };
    }

    rtu::RestClient::ErrorCallback makeErrorCalback(const rtu::OperationCallback &callback
        , rtu::Constants::AffectedEntities affected)
    {
        return [callback, affected](int error)
        {
            if (callback)
                callback(error, affected);
        };
    }

    ///

    void checkAuth(const rtu::BaseServerInfo &baseInfo
        , const QString &password
        , const rtu::OperationCallback &callback
        , int timeout = rtu::HttpClient::kUseDefaultTimeout)
    {
        static const rtu::Constants::AffectedEntities affected = rtu::Constants::kNoEntitiesAffected;

        const rtu::RestClient::Request request(baseInfo, password, kModuleInfoAuthCommand, QUrlQuery(), timeout
            , makeSuccessCalback(callback, affected), makeErrorCalback(callback, affected));
        rtu::RestClient::sendGet(request);
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

bool rtu::parseModuleInformationReply(const QJsonObject &reply
    , rtu::BaseServerInfo &baseInfo)
{
    if (isErrorReply(reply))
        return false;

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
    , const OperationCallback &failedCallback)
{
    static const Constants::AffectedEntities affected = (Constants::kAllEntitiesAffected 
        & ~Constants::kAllAddressFlagsAffected);

    BaseServerInfo info;
    info.id = id;
    info.discoveredByHttp = false;  /// force multicast usage

    const auto &successfull = [info, successCallback, failedCallback](const QByteArray &data)
    {
        const QJsonObject object = QJsonDocument::fromJson(data.data()).object();
        BaseServerInfo resultInfo;
        if (!parseModuleInformationReply(object, resultInfo))
        {
            if (failedCallback)
                failedCallback(kUnspecifiedError, affected);
            return;
        }

        if (successCallback)
            successCallback(resultInfo);
    };

    const QString &kModuleInformationCommand = kApiNamespaceTag + "moduleInformation";
    RestClient::Request request(info, kModuleInformationCommand, QUrlQuery(), RestClient::kStandardTimeout
        , successfull, makeErrorCalback(failedCallback, affected));
    RestClient::sendGet(request);
}

/// 

void rtu::getTime(const BaseServerInfo &baseInfo
    , const QString &password
    , const ExtraServerInfoSuccessCallback &successful
    , const OperationCallback &failed
    , int timeout)
{
    static const Constants::AffectedEntities affected = 
        (Constants::kTimeZoneAffected | Constants::kDateTimeAffected);

    QUrlQuery query;
    if (baseInfo.flags & Constants::AllowChangeDateTimeFlag)
        query.addQueryItem(kLocalTimeFlagTag, QString());

    const auto &parser = [](const QJsonObject &object, ExtraServerInfo &extraInfo)
        { return parseGetTimeCmd(object, extraInfo); };

    const auto &localSuccessful = makeReplyCallbackEx(
        successful, failed, baseInfo.id, password, affected, parser);

    const RestClient::Request request(baseInfo, password, kGetTimeCommand, query,  timeout
        , localSuccessful, makeErrorCalback(failed, affected));

    RestClient::sendGet(request);
}

///

void rtu::getIfList(const BaseServerInfo &baseInfo
    , const QString &password
    , const ExtraServerInfoSuccessCallback &successful
    , const OperationCallback &failed
    , int timeout)
{
    static const Constants::AffectedEntities affected = Constants::kAllAddressFlagsAffected;

    const auto &parser = [](const QJsonObject &object, ExtraServerInfo &extraInfo)
        { return parseIfListCmd(object, extraInfo); };

    const auto &ifListSuccessfull = makeReplyCallbackEx(
        successful, failed, baseInfo.id, password, affected, parser);

    const RestClient::Request request(baseInfo, password, kIfListCommand, QUrlQuery(),  timeout
        , ifListSuccessfull, makeErrorCalback(failed, affected));
    RestClient::sendGet(request);
}

///

void rtu::getServerExtraInfo(const BaseServerInfo &baseInfo
    , const QString &password
    , const ExtraServerInfoSuccessCallback &successful
    , const OperationCallback &failed
    , int timeout)
{
    static const Constants::AffectedEntities affected = (Constants::kAllAddressFlagsAffected 
        | Constants::kTimeZoneAffected | Constants::kDateTimeAffected);

    const auto &getTimeFailed = [failed](const int errorCode, Constants::AffectedEntities /* affectedEntities */)
    {
        if (failed)
            failed(errorCode, affected);
    };

    const auto &getTimeSuccessfull = [baseInfo, password, successful, timeout]
        (const QUuid &id, const rtu::ExtraServerInfo &extraInfo) 
    {
        /// getTime command is successfully executed up to now
        const auto &ifListFailed = [successful, id, extraInfo](const int, Constants::AffectedEntities  /* affectedEntities */)
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

        getIfList(baseInfo, password, ifListSuccessful, ifListFailed, timeout);
    };

    const auto &callback = [failed, baseInfo, password, getTimeSuccessfull, getTimeFailed, timeout]
        (const int errorCode, Constants::AffectedEntities /* affectedEntities */)
    {
        if (errorCode != kNoErrorReponse)
        {
            if (failed)
                failed(errorCode, affected);
            return;
        }

        getTime(baseInfo, password, getTimeSuccessfull, getTimeFailed, timeout);
    };

    return checkAuth(baseInfo, password, callback, timeout);
}

///

void rtu::sendIfListRequest(const BaseServerInfo &info
    , const QString &password
    , const ExtraServerInfoSuccessCallback &successful
    , const OperationCallback &failed
    , int timeout)
{
    if (!(info.flags & Constants::AllowIfConfigFlag))
    {
        if (failed)
            failed(kUnspecifiedError, Constants::kAllEntitiesAffected);
        return;
    }
 
    const auto &parser = [](const QJsonObject &object, ExtraServerInfo &extraInfo)
        { return parseIfListCmd(object, extraInfo); };
    const auto &localSuccessful = makeReplyCallbackEx(successful, failed, info.id
        , password, Constants::kAllAddressFlagsAffected, parser);
    const RestClient::Request request(info, password, kIfListCommand, QUrlQuery(), timeout
        , localSuccessful, makeErrorCalback(failed, Constants::kAllAddressFlagsAffected));
    RestClient::sendGet(request);
}

///

void rtu::sendSetTimeRequest(const ServerInfo &info
    , qint64 utcDateTimeMs
    , const QByteArray &timeZoneId
    , const OperationCallback &callback)
{
    static const Constants::AffectedEntities affected = (Constants::kDateTimeAffected | Constants::kTimeZoneAffected);
    if (!info.hasExtraInfo() || utcDateTimeMs <= 0
        || !QTimeZone(timeZoneId).isValid())
    {
        if (callback)
            callback(kUnspecifiedError, affected);
        return;
    }
    
    static const QString &kDateTimeTag = "datetime";
    static const QString &kTimeZoneTag = "timezone";
    
    QUrlQuery query;
    query.addQueryItem(kDateTimeTag, QString::number(utcDateTimeMs));
    query.addQueryItem(kTimeZoneTag, timeZoneId);
    
    const RestClient::Request request(info, kSetTimeCommand, query, RestClient::kStandardTimeout
        , makeSuccessCalback(callback, affected), makeErrorCalback(callback, affected)); 
    RestClient::sendGet(request);
}

///

void rtu::sendSetSystemNameRequest(const ServerInfo &info
    , const QString &systemName
    , const OperationCallback &callback)
{
    if (!info.hasExtraInfo() || systemName.isEmpty())
    {
        if (callback)
            callback(kUnspecifiedError, Constants::kSystemNameAffected);
        return;
    }

    static const QString kSystemNameTag = "systemName";
    static const QString oldPasswordTag = "oldPassword";

    QUrlQuery query;
    query.addQueryItem(kSystemNameTag, systemName);
    query.addQueryItem(oldPasswordTag, info.extraInfo().password);

    const RestClient::Request request(info, kConfigureCommand, query, RestClient::kStandardTimeout
        , makeSuccessCalback(callback, Constants::kSystemNameAffected)
        , makeErrorCalback(callback, Constants::kSystemNameAffected)); 
    RestClient::sendGet(request);
}

///

void rtu::sendSetPasswordRequest(const ServerInfo &info
    , const QString &password
    , bool useNewPassword
    , const OperationCallback &callback)
{
    if (!info.hasExtraInfo() || password.isEmpty())
    {
        if (callback)
            callback(kUnspecifiedError, Constants::kPasswordAffected);

        return;
    }

    static const QString newPasswordTag = "password";
    static const QString oldPasswordTag = "oldPassword";
    
    const QString authPass = (useNewPassword ? password : info.extraInfo().password);
    QUrlQuery query;
    query.addQueryItem(newPasswordTag, password);
    query.addQueryItem(oldPasswordTag, authPass);
    
    const RestClient::Request request(info.baseInfo(), authPass, kConfigureCommand, query, RestClient::kStandardTimeout
        , makeSuccessCalback(callback, Constants::kPasswordAffected)
        , makeErrorCalback(callback, Constants::kPasswordAffected)); 
    RestClient::sendGet(request);
}

///

void rtu::sendSetPortRequest(const ServerInfo &info
    , int port
    , const OperationCallback &callback)
{
    if (!info.hasExtraInfo() || !port)
    {
        if (callback)
            callback(kUnspecifiedError, Constants::kPortAffected);
       return;
    }

    static const QString oldPasswordTag = "oldPassword";

    QUrlQuery query;
    query.addQueryItem(kPortTag, QString::number(port));
    query.addQueryItem(oldPasswordTag, info.extraInfo().password);

    QUrl url = makeUrl(info, kConfigureCommand);
    url.setQuery(query);

    const RestClient::Request request(info, kConfigureCommand, query, RestClient::kStandardTimeout
        , makeSuccessCalback(callback, Constants::kPortAffected)
        , makeErrorCalback(callback, Constants::kPortAffected)); 
    RestClient::sendGet(request);
}

void rtu::sendChangeItfRequest(const ServerInfo &info
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

    if (info.hasExtraInfo())
    {
        const RestClient::Request request(info, kIfConfigCommand, QUrlQuery(), RestClient::kStandardTimeout
            , makeSuccessCalback(callback, affected), makeErrorCalback(callback, affected));
        RestClient::sendPost(request, QJsonDocument(jsonInfoChanges).toJson());
    }
    else if (callback)
    {
        callback(kUnspecifiedError, affected);
    }
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

