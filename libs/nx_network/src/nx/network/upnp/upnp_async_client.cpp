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
        const QStringRef& /*namespaceUri*/,
        const QStringRef& localName,
        const QXmlStreamAttributes& /*attrs*/)
    {
        if (localName == "xml" || localName == "Envelope")
            return true; // TODO: check attrs

        m_awaitedValue = &m_message.params[localName.toString()];
        return true;
    }

    virtual bool endElement(
        const QStringRef& /*namespaceUri*/,
        const QStringRef& /*localName*/)
    {
        m_awaitedValue = 0;
        return true;
    }

    virtual bool characters(const QStringRef& ch)
    {
        if (m_awaitedValue)
            *m_awaitedValue = ch.toString();

        return true;
    }

    const AsyncClient::Message& message() const { return m_message; }

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
        const QStringRef& namespaceUri,
        const QStringRef& localName,
        const QXmlStreamAttributes& attrs) override
    {
        if (localName == "Body")
            return true;

        if (namespaceUri == "u" || namespaceUri.startsWith("u:"))
        {
            m_message.action = localName.toString();
            m_message.service = fromUpnpUrn(namespaceUri.toString(), "service");
            return true;
        }

        return base_type::startElement(namespaceUri, localName, attrs);
    }
};

class UpnpFailureHandler:
    public UpnpMessageHandler
{
    using base_type = UpnpMessageHandler;

public:
    virtual bool startElement(
        const QStringRef& namespaceUri,
        const QStringRef& localName,
        const QXmlStreamAttributes& attrs) override
    {
        if (localName == "Fault" || localName == "UPnpError" || localName == "detail")
            return true;

        return base_type::startElement(namespaceUri, localName, attrs);
    }
};

void fetchMappingsRecursive(
    AsyncClient* client,
    const nx::utils::Url& url,
    const std::function<void(AsyncClient::MappingList)>& callback,
    std::shared_ptr<AsyncClient::MappingList> collected,
    AsyncClient::MappingInfo newMap)
{
    if (!newMap.isValid())
    {
        callback(std::move(*collected));
        return;
    }

    collected->push_back(std::move(newMap));
    client->getMapping(url, collected->size(),
        [client, url, callback, collected](AsyncClient::MappingInfo nextMap)
        {
            fetchMappingsRecursive(client, url, callback, collected, std::move(nextMap));
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

AsyncClient::MappingInfo::MappingInfo(
    const HostAddress& inIp,
    quint16 inPort,
    quint16 exPort,
    Protocol prot,
    const QString& desc,
    quint64 dur)
    :
    internalIp(inIp),
    internalPort(inPort),
    externalPort(exPort),
    protocol(prot),
    description(desc),
    duration(dur)
{
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

void AsyncClient::doUpnp(const nx::utils::Url& url, const Message& message,
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

        if (auto resp = ptr->response())
        {
            const auto status = resp->statusLine.statusCode;
            const bool success = (status >= 200 && status < 300);

            std::unique_ptr<UpnpMessageHandler> xmlHandler;
            if (success)
                xmlHandler.reset(new UpnpSuccessHandler);
            else
                xmlHandler.reset(new UpnpFailureHandler);

            QXmlStreamReader reader(toByteArray(ptr->fetchMessageBodyBuffer()));
            if (nx::utils::parseXml(reader, *xmlHandler))
            {
                callback(xmlHandler->message());
                return;
            }
        }

        NX_ERROR(this, nx::format("Could not parse message from %1").arg(
            url.toString(QUrl::RemovePassword)));

        callback(Message());
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
    const nx::utils::Url& url,
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
    const nx::utils::Url& url, const HostAddress& internalIp, quint16 internalPort,
    quint16 externalPort, Protocol protocol, const QString& description,
    quint64 duration, std::function<void(bool)> callback)
{
    AsyncClient::Message request(kAddPortMapping, kWanIp);
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
            callback(response.isOk());
        });
}

void AsyncClient::deleteMapping(
    const nx::utils::Url& url, quint16 externalPort, Protocol protocol,
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
    const nx::utils::Url& url, quint32 index,
    std::function<void(MappingInfo)> callback)
{
    AsyncClient::Message request(kGetGenericPortMappingEntry, kWanIp);
    request.params[kNewPortMappingIndex] = QString::number(index);

    return doUpnp(
        url,
        request,
        [callback](const Message& response)
        {
            if (response.isOk())
                callback(MappingInfo(
                    response.getParam(kNewInternalClient).toStdString(),
                    response.getParam(kNewInternalPort).toUShort(),
                    response.getParam(kNewExternalPort).toUShort(),
                    nx::reflect::fromString<Protocol>(
                        response.getParam(kNewProtocol).toStdString()),
                    response.getParam(kNewPortMappingDescription)));
            else
                callback(MappingInfo());
        });
}

void AsyncClient::getMapping(
    const nx::utils::Url& url, quint16 externalPort, Protocol protocol,
    std::function<void(MappingInfo)> callback)
{
    AsyncClient::Message request(kGetSpecificPortMappingEntry, kWanIp);
    request.params[kNewExternalPort] = QString::number(externalPort);
    request.params[kNewProtocol] = nx::toString(protocol);

    return doUpnp(url, request,
        [callback, externalPort, protocol](const Message& response)
        {
            if (response.isOk())
            {
                callback(MappingInfo(
                    response.getParam(kNewInternalClient).toStdString(),
                    response.getParam(kNewInternalPort).toUShort(),
                    externalPort, protocol,
                    response.getParam(kNewPortMappingDescription),
                    response.getParam(kNewLeaseDuration).toULongLong()));
            }
            else
            {
                callback(MappingInfo());
            }
        });
}

void AsyncClient::getAllMappings(
    const nx::utils::Url& url, std::function<void(MappingList)> callback)
{
    auto mappings = std::make_shared<std::vector<MappingInfo>>();
    return getMapping(url, 0,
        [this, url, callback, mappings](MappingInfo newMap)
        {
            fetchMappingsRecursive(this, url, callback, mappings, std::move(newMap));
        });
}

} // namespace nx::network::upnp
