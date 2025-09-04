// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "upnp_async_client.h"

#include <nx/reflect/string_conversion.h>
#include <nx/utils/log/log.h>
#include <nx/utils/serialization/qt_xml_helper.h>

#include "upnp_device_description.h"

namespace nx::network::upnp {

namespace {

static constexpr std::chrono::seconds kMessageBodyReadTimeout(20);

static const QString kSoapRequest(
    "<?xml version=\"1.0\" ?>"
    "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\""
    " s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
    "<s:Body>"
    "<u:%1 xmlns:u=\"%2\">"
    "%3" // params
    "</u:%1>"
    "</s:Body>"
    "</s:Envelope>"
);

static const QString kNone; //< Just an empty string.

static const QString kAnyRemoteHost; //< Empty string = allow any remote host.

static const QString kErrorCodeParameter = "errorCode";

// UPNP parameters.
static const QString kGetExternalIPAddress("GetExternalIPAddress");
static const QString kAddPortMapping("AddPortMapping");
static const QString kDeletePortMapping("DeletePortMapping");
static const QString kGetGenericPortMappingEntry("GetGenericPortMappingEntry");
static const QString kGetSpecificPortMappingEntry("GetSpecificPortMappingEntry");

static const QString kNewPortMappingIndex("NewPortMappingIndex");
static const QString kNewExternalIPAddress("NewExternalIPAddress");
static const QString kNewExternalPort("NewExternalPort");
static const QString kNewProtocol("NewProtocol");
static const QString kNewInternalPort("NewInternalPort");
static const QString kNewInternalClient("NewInternalClient");
static const QString kNewEnabled("NewEnabled");
static const QString kNewPortMappingDescription("NewPortMappingDescription");
static const QString kNewLeaseDuration("NewLeaseDuration");
static const QString kNewRemoteHost("NewRemoteHost");

// There are evidences that some routers send XMLs with broken namespaces, for example:
// `<u:DeletePortMappingResponse xmlns:u0="urn:schemas-upnp-org:service:WANIPConnection:1">`.
// This makes XML invalid, but we can workaround the known cases.
const QXmlStreamNamespaceDeclarations& knownBrokenNamespaces()
{
    static const QXmlStreamNamespaceDeclarations namespaces =
        []()
        {
            const QString urn = toUpnpUrn(AsyncClient::kWanIp, "service");
            QXmlStreamNamespaceDeclarations namespaces;
            // Add namespace prefixes here.
            for (QString prefix: {"u"})
                namespaces.emplaceBack(prefix, urn);
            return namespaces;
        }();
    return namespaces;
};

// TODO: Move parsers to separate file.
class UpnpMessageHandler
{
public:
    virtual ~UpnpMessageHandler() = default;

    virtual bool startDocument()
    {
        m_message = AsyncClient::Message();
        m_awaitedValue = 0;
        return true;
    }

    virtual bool endDocument() { return true; }

    virtual bool startElement(
        const QStringView& /*namespaceUri*/,
        const QStringView& localName,
        const QXmlStreamAttributes& /*attrs*/)
    {
        if (isIgnoredElement(localName))
            return true;

        m_awaitedValue = &m_message.params[localName.toString()];
        return true;
    }

    virtual bool endElement(
        const QStringView& /*namespaceUri*/,
        const QStringView& /*localName*/)
    {
        m_awaitedValue = nullptr;
        return true;
    }

    virtual bool characters(const QStringView& ch)
    {
        if (m_awaitedValue)
            *m_awaitedValue = ch.toString();

        return true;
    }

    const AsyncClient::Message& message() const { return m_message; }

protected:
    virtual QStringList ignoredElements() const
    {
        return {"xml", "Envelope", "Body"};
    }

    bool isIgnoredElement(QStringView name) const
    {
        return ignoredElements().contains(name, Qt::CaseInsensitive);
    }

protected:
    AsyncClient::Message m_message;
    QString* m_awaitedValue;
};

class UpnpSuccessHandler:
    public UpnpMessageHandler
{
    using base_type = UpnpMessageHandler;

public:
    virtual bool startElement(
        const QStringView& namespaceUri,
        const QStringView& localName,
        const QXmlStreamAttributes& attrs) override
    {
        static constexpr QStringView kResponseSuffix = u"Response";
        if (localName.endsWith(kResponseSuffix))
        {
            m_message.action = localName.chopped(kResponseSuffix.size()).toString();
            m_message.service = fromUpnpUrn(namespaceUri.toString(), u"service");
            return true;
        }

        return base_type::startElement(namespaceUri, localName, attrs);
    }
};

class UpnpFailureHandler:
    public UpnpMessageHandler
{
    using base_type = UpnpMessageHandler;

protected:
    virtual QStringList ignoredElements() const override
    {
        return base_type::ignoredElements() + QStringList{"Fault", "UPnPError", "detail"};
    }
};

void fetchMappingsRecursive(
    AsyncClient* client,
    const nx::Url& url,
    const AsyncClient::MappingInfoListCallback& callback,
    std::shared_ptr<AsyncClient::MappingInfoList> collected,
    AsyncClient::MappingInfoResult result)
{
    if (!result)
    {
        // When mapping list is exhausted, specifiedArrayIndexInvalid is returned.
        // This means we fetched all exisitng mappings from the device.
        callback(
            std::move(*collected),
            result.error() == AsyncClient::ErrorCode::specifiedArrayIndexInvalid);
        return;
    }

    collected->push_back(std::move(*result));
    client->getMapping(url, collected->size(),
        [client, url, callback, collected](AsyncClient::MappingInfoResult result)
        {
            fetchMappingsRecursive(client, url, callback, collected, std::move(result));
        });
}

} // namespace

bool AsyncClient::Message::isOk() const
{
    return !action.isEmpty() && !service.isEmpty();
}

const QString& AsyncClient::Message::getParam(const QString& key) const
{
    const auto it = params.find(key);
    return (it == params.end()) ? kNone : it->second;
}

QString AsyncClient::Message::toString() const
{
    QStringList paramList;
    for (const auto& param: params)
        paramList << nx::format("%1='%2'").args(param.first, param.second);

    return nx::format("%1:%2(%3)").args(service, action, paramList.join(", "));
}

bool AsyncClient::MappingInfo::isValid() const
{
    return !(internalIp == HostAddress()) && internalPort && externalPort;
}

QString AsyncClient::MappingInfo::toString() const
{
    return nx::format("MappingInfo( %1:%2 -> %3 %4 : %5 for %6 )",
        internalIp.toString(),
        internalPort,
        externalPort,
        protocol,
        description,
        std::chrono::milliseconds(duration).count());
}

AsyncClient::~AsyncClient()
{
    std::set<nx::network::http::AsyncHttpClientPtr> httpClients;
    {
        NX_MUTEX_LOCKER lk(&m_mutex);
        m_isTerminating = true;
        std::swap(httpClients, m_httpClients);
    }

    for (const auto& client : httpClients)
        client->pleaseStopSync();
}

void AsyncClient::doUpnp(const nx::Url& url, const Message& message,
    std::function<void(const Message&)> callback)
{
    const auto service = toUpnpUrn(message.service, "service");
    const auto action = QString("\"%1#%2\"").arg(service, message.action);

    QStringList params;
    for (const auto& p : message.params)
        params.push_back(QString("<%1>%2</%1>").arg(p.first, p.second));

    const auto request = kSoapRequest.arg(message.action, service, params.join(""));

    auto complete = [this, url, callback](const nx::network::http::AsyncHttpClientPtr& ptr)
    {
        {
            NX_MUTEX_LOCKER lk(&m_mutex);
            m_httpClients.erase(ptr);
        }

        std::optional<Message> result;
        if (auto resp = ptr->response())
        {
            result = parseResponse(
                resp->statusLine.statusCode,
                ptr->fetchMessageBodyBuffer().toByteArray());
        }

        if (!result)
            NX_ERROR(this, "Could not parse message from %1", url.toString(QUrl::RemovePassword));

        callback(result.value_or(Message()));
    };

    NX_MUTEX_LOCKER lk(&m_mutex);
    if (m_isTerminating)
        return;

    const auto httpClient = http::AsyncHttpClient::create(ssl::kAcceptAnyCertificate);
    httpClient->addAdditionalHeader("SOAPAction", action.toStdString());
    httpClient->setMessageBodyReadTimeoutMs(
        std::chrono::milliseconds(kMessageBodyReadTimeout).count());
    QObject::connect(httpClient.get(), &nx::network::http::AsyncHttpClient::done,
        httpClient.get(), std::move(complete), Qt::DirectConnection);

    m_httpClients.insert(httpClient);
    httpClient->doPost(url, "text/xml", nx::Buffer(request.toStdString()));
}

void AsyncClient::externalIp(
    const nx::Url& url,
    std::function< void(const HostAddress&) > callback)
{
    AsyncClient::Message request(kGetExternalIPAddress, kWanIp);
    doUpnp(
        url,
        request,
        [callback](const Message& response)
        {
            callback(response.getParam(kNewExternalIPAddress).toStdString());
        });
}

void AsyncClient::addMapping(
    const nx::Url& url,
    const HostAddress& internalIp,
    quint16 internalPort,
    quint16 externalPort,
    Protocol protocol,
    const QString& description,
    quint64 duration,
    std::function<void(ErrorCode)> callback)
{
    AsyncClient::Message request(kAddPortMapping, kWanIp);
    request.params[kNewRemoteHost] = kAnyRemoteHost;
    request.params[kNewExternalPort] = QString::number(externalPort);
    request.params[kNewProtocol] = nx::toString(protocol);
    request.params[kNewInternalPort] = QString::number(internalPort);
    request.params[kNewInternalClient] = QString::fromStdString(internalIp.toString());
    request.params[kNewEnabled] = QString::number(1);
    request.params[kNewPortMappingDescription] = description.isEmpty() ? kClientId : description;
    request.params[kNewLeaseDuration] = QString::number(duration);

    doUpnp(
        url,
        request,
        [callback](const Message& response)
        {
            callback(response.isOk() ? ErrorCode::ok : getErrorCode(response));
        });
}

void AsyncClient::deleteMapping(
    const nx::Url& url, quint16 externalPort, Protocol protocol,
    std::function<void(bool)> callback)
{
    AsyncClient::Message request(kDeletePortMapping, kWanIp);
    request.params[kNewExternalPort] = QString::number(externalPort);
    request.params[kNewProtocol] = nx::toString(protocol);

    doUpnp(
        url,
        request,
        [callback](const Message& response)
        {
            callback(response.isOk());
        });
    }

void AsyncClient::getMapping(
    const nx::Url& url, quint32 index,
    MappingInfoCallback callback)
{
    AsyncClient::Message request(kGetGenericPortMappingEntry, kWanIp);
    request.params[kNewPortMappingIndex] = QString::number(index);

    return doUpnp(
        url,
        request,
        [callback](const Message& response)
        {
            processMappingResponse(response, callback);
        });
}

void AsyncClient::getMapping(
    const nx::Url& url, quint16 externalPort, Protocol protocol,
    MappingInfoCallback callback)
{
    AsyncClient::Message request(kGetSpecificPortMappingEntry, kWanIp);
    request.params[kNewExternalPort] = QString::number(externalPort);
    request.params[kNewProtocol] = nx::toString(protocol);
    request.params[kNewRemoteHost] = kAnyRemoteHost;

    return doUpnp(
        url,
        request,
        [callback, externalPort, protocol](const Message& response)
        {
            processMappingResponse(response, callback, protocol, externalPort);
        });
}

AsyncClient::ErrorCode AsyncClient::getErrorCode(const Message& message)
{
    bool ok = false;
    const int code = message.getParam(kErrorCodeParameter).toInt(&ok);
    return ok ? ErrorCode(code) : ErrorCode::unknown;
}

void AsyncClient::getAllMappings(
    const nx::Url& url, MappingInfoListCallback callback)
{
    auto mappings = std::make_shared<MappingInfoList>();
    return getMapping(url, 0,
        [this, url, callback, mappings](MappingInfoResult result)
        {
            fetchMappingsRecursive(this, url, callback, mappings, std::move(result));
        });
}

void AsyncClient::processMappingResponse(
    const Message& response,
    MappingInfoCallback callback,
    std::optional<Protocol> protocol,
    std::optional<quint16> externalPort)
{
    if (response.isOk())
    {
        callback(MappingInfo{
            .internalIp = HostAddress::fromString(
                response.getParam(kNewInternalClient).toStdString()),
            .internalPort = response.getParam(kNewInternalPort).toUShort(),
            .externalPort = externalPort.value_or(response.getParam(kNewExternalPort).toUShort()),
            .protocol = protocol.value_or(
                nx::reflect::fromString<Protocol>(response.getParam(kNewProtocol).toStdString())),
            .description = response.getParam(kNewPortMappingDescription),
            .duration = std::chrono::milliseconds(
                response.getParam(kNewLeaseDuration).toULongLong()),
        });
    }
    else
    {
        callback(std::unexpected(getErrorCode(response)));
    }
}

std::optional<AsyncClient::Message> AsyncClient::parseResponse(
    nx::network::http::StatusCode::Value statusCode, const QByteArray& responseBody)
{
    std::unique_ptr<UpnpMessageHandler> xmlHandler;
    if (nx::network::http::StatusCode::isSuccessCode(statusCode))
        xmlHandler = std::make_unique<UpnpSuccessHandler>();
    else
        xmlHandler = std::make_unique<UpnpFailureHandler>();

    QXmlStreamReader reader(responseBody);
    reader.addExtraNamespaceDeclarations(knownBrokenNamespaces());
    if (nx::utils::parseXml(reader, *xmlHandler))
        return xmlHandler->message();
    return std::nullopt;
}

} // namespace nx::network::upnp
