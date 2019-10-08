#include "upnp_async_client.h"

#include <nx/fusion/serialization/lexical_functions.h>
#include <nx/utils/log/log.h>

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

// TODO: move parsers to separate file
class UpnpMessageHandler:
    public QXmlDefaultHandler
{
public:
    virtual bool startDocument() override
    {
        m_message = AsyncClient::Message();
        m_awaitedValue = 0;
        return true;
    }

    virtual bool endDocument() override { return true; }

    virtual bool startElement(const QString& /*namespaceURI*/,
        const QString& localName,
        const QString& qName,
        const QXmlAttributes& /*attrs*/) override
    {
        if (localName == "xml" || localName == "Envelope")
            return true; // TODO: check attrs

        m_awaitedValue = &m_message.params[qName];
        return true;
    }

    virtual bool endElement(const QString& /*namespaceURI*/,
        const QString& /*localName*/,
        const QString& /*qName*/) override
    {
        m_awaitedValue = 0;
        return true;
    }

    virtual bool characters(const QString& ch) override
    {
        if (m_awaitedValue)
            *m_awaitedValue = ch;

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
    typedef UpnpMessageHandler super;

public:
    virtual bool startDocument() override
    {
        return super::startDocument();
    }

    virtual bool startElement(const QString& namespaceURI,
        const QString& localName,
        const QString& qName,
        const QXmlAttributes& attrs) override
    {
        if (localName == "Body")
            return true;

        if (qName.startsWith("u:"))
        {
            m_message.action = localName;
            m_message.service = fromUpnpUrn(namespaceURI, "service");
            return true;
        }

        return super::startElement(namespaceURI, localName, qName, attrs);
    }
};

class UpnpFailureHandler:
    public UpnpMessageHandler
{
    typedef UpnpMessageHandler super;

public:
    virtual bool startElement(const QString& namespaceURI,
        const QString& localName,
        const QString& qName,
        const QXmlAttributes& attrs) override
    {
        if (localName == "Fault" || localName == "UPnpError" ||
            localName == "detail")
            return true;

        return super::startElement(namespaceURI, localName, qName, attrs);
    }
};

void fetchMappingsRecursive(
    AsyncClient* client, const nx::utils::Url& url,
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
        paramList << lm("%1='%2'").args(param.first, param.second);

    return lm("%1:%2(%3)").args(service, action, paramList.join(", "));
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
    return lm("MappingInfo( %1:%2 -> %3 %4 : %5 for %6 )")
        .arg(internalIp.toString()).arg(internalPort)
        .arg(externalPort).arg(QnLexical::serialized(protocol))
        .arg(description).arg(std::chrono::milliseconds(duration).count());
}

AsyncClient::~AsyncClient()
{
    std::set<nx::network::http::AsyncHttpClientPtr> httpClients;
    {
        QnMutexLocker lk(&m_mutex);
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
            QnMutexLocker lk(&m_mutex);
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

            QXmlSimpleReader xmlReader;
            xmlReader.setContentHandler(xmlHandler.get());
            xmlReader.setErrorHandler(xmlHandler.get());

            QXmlInputSource input;
            input.setData(ptr->fetchMessageBodyBuffer());
            if (xmlReader.parse(&input))
            {
                callback(xmlHandler->message());
                return;
            }
        }

        NX_ERROR(this, lm("Could not parse message from %1").arg(
            url.toString(QUrl::RemovePassword)));

        callback(Message());
    };

    QnMutexLocker lk(&m_mutex);
    if (m_isTerminating)
        return;

    const auto httpClient = nx::network::http::AsyncHttpClient::create();
    httpClient->addAdditionalHeader("SOAPAction", action.toUtf8());
    httpClient->setMessageBodyReadTimeoutMs(
        std::chrono::milliseconds(kMessageBodyReadTimeout).count());
    QObject::connect(httpClient.get(), &nx::network::http::AsyncHttpClient::done,
        httpClient.get(), std::move(complete), Qt::DirectConnection);

    m_httpClients.insert(httpClient);
    httpClient->doPost(url, "text/xml", request.toUtf8());
}

void AsyncClient::externalIp(
    const nx::utils::Url& url, std::function<void(const HostAddress&)> callback)
{
    AsyncClient::Message request(kGetExternalIPAddress, kWanIp);
    doUpnp(
        url,
        request,
        [callback](const Message& response)
        {
            callback(response.getParam(kNewExternalIPAddress));
        });
}

void AsyncClient::addMapping(
    const nx::utils::Url& url, const HostAddress& internalIp, quint16 internalPort,
    quint16 externalPort, Protocol protocol, const QString& description,
    quint64 duration, std::function<void(bool)> callback)
{
    AsyncClient::Message request(kAddPortMapping, kWanIp);
    request.params[kNewExternalPort] = QString::number(externalPort);
    request.params[kNewProtocol] = QnLexical::serialized(protocol);
    request.params[kNewInternalPort] = QString::number(internalPort);
    request.params[kNewInternalClient] = internalIp.toString();
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
    request.params[kNewProtocol] = QnLexical::serialized(protocol);

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
                    response.getParam(kNewInternalClient),
                    response.getParam(kNewInternalPort).toUShort(),
                    response.getParam(kNewExternalPort).toUShort(),
                    QnLexical::deserialized<Protocol>(response.getParam(kNewProtocol)),
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
    request.params[kNewProtocol] = QnLexical::serialized(protocol);

    return doUpnp(url, request,
        [callback, externalPort, protocol](const Message& response)
        {
            if (response.isOk())
            {
                callback(MappingInfo(
                    response.getParam(kNewInternalClient),
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

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::network::upnp::AsyncClient, Protocol,
    (nx::network::upnp::AsyncClient::Protocol::TCP, "tcp")
    (nx::network::upnp::AsyncClient::Protocol::UDP, "udp")
)
