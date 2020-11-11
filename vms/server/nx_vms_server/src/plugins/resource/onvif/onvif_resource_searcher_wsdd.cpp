#ifdef ENABLE_ONVIF

#include <openssl/evp.h>

#include <nx/utils/uuid.h>

#include <nx/network/nettools.h>
#include "nx/utils/string.h"
#include <nx/utils/log/log.h>

#include "core/resource/camera_resource.h"

#include <gsoap/wsaapi.h>
#include "onvif_resource_searcher_wsdd.h"
#include "onvif_resource.h"
#include "onvif_helper.h"
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>

#include <nx/fusion/model_functions.h>

//static const int SOAP_DISCOVERY_TIMEOUT = 1; // "+" in seconds, "-" in milliseconds
static const int SOAP_DISCOVERY_TIMEOUT = -500; // "+" in seconds, "-" in milliseconds
static const int SOAP_HELLO_CHECK_TIMEOUT = -1; // "+" in seconds, "-" in milliseconds
static const int CHECK_HELLO_RETRY_COUNT = 50;

QString OnvifResourceSearcherWsdd::LOCAL_ADDR(QLatin1String("127.0.0.1"));
const char OnvifResourceSearcherWsdd::SCOPES_NAME_PREFIX[] = "onvif://www.onvif.org/name/";
const char OnvifResourceSearcherWsdd::SCOPES_HARDWARE_PREFIX[] = "onvif://www.onvif.org/hardware/";
const char OnvifResourceSearcherWsdd::SCOPES_LOCATION_PREFIX[] = "onvif://www.onvif.org/location/";
const static std::array<const char*, 3> kProbeTypes =
{
    "NetworkVideoTransmitter",
    "NetworkVideoDisplay",
    "Device"
};
const char OnvifResourceSearcherWsdd::WSA_ADDRESS[] =
    "http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous";
const char OnvifResourceSearcherWsdd::WSDD_ADDRESS[] =
    "urn:schemas-xmlsoap-org:ws:2005:04:discovery";
const char OnvifResourceSearcherWsdd::WSDD_ACTION[] =
    "http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe";

const QString  OnvifResourceSearcherWsdd::OBTAIN_MAC_FROM_MULTICAST_PARAM_NAME =
    lit("obtainMacFromMulticast");

static const std::vector<QString> kManufacturerScopePrefixes
    = {lit("onvif://www.onvif.org/manufacturer/")};

const char OnvifResourceSearcherWsdd::WSDD_GSOAP_MULTICAST_ADDRESS[] = "soap.udp://239.255.255.250:3702";

static const int WSDD_MULTICAST_PORT = 3702;
static const char WSDD_MULTICAST_ADDRESS[] = "239.255.255.250";
static const nx::network::SocketAddress WSDD_MULTICAST_ENDPOINT(WSDD_MULTICAST_ADDRESS, WSDD_MULTICAST_PORT);

namespace
{
    //To avoid recreating of gsoap socket, these 2 functions must be assigned
    int nullGsoapFconnect(struct soap*, const char*, const char*, int)
    {
        return SOAP_OK;
    }

    int nullGsoapFdisconnect(struct soap*)
    {
        return SOAP_OK;
    }
    static const char STATIC_DISCOVERY_MESSAGE[] = "\
<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" xmlns:a=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\">\
<s:Header>\
<a:Action s:mustUnderstand=\"1\">\
http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe\
</a:Action>\
<a:MessageID>%1</a:MessageID>\
<a:ReplyTo>\
<a:Address>\
http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous\
</a:Address>\
</a:ReplyTo>\
<a:To s:mustUnderstand=\"1\">urn:schemas-xmlsoap-org:ws:2005:04:discovery</a:To>\
</s:Header>\
<s:Body>\
<Probe xmlns=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\">\
<d:Types xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" xmlns:dp0=\"http://www.onvif.org/ver10/network/wsdl\">dp0:%2</d:Types>\
</Probe>\
</s:Body>\
</s:Envelope>";

    // avoid SOAP select call
    size_t gsoapFrecv(struct soap* soap, char* data, size_t maxSize)
    {
        nx::network::AbstractDatagramSocket* qSocket = reinterpret_cast<nx::network::AbstractDatagramSocket*>(soap->user);
        int readed = qSocket->recv(data, static_cast<unsigned int>(maxSize), 0);
        return (size_t) qMax(0, readed);
    }

    //Socket send through UDPSocket
    int gsoapFsendSmall(struct soap *soap, const char *s, size_t n)
    {
        //avoiding sending numerous data
        if (!QByteArray::fromRawData(s, static_cast<int>(n)).startsWith("<?xml")) {
            return SOAP_OK;
        }

        QString msgId;
        nx::network::AbstractDatagramSocket* qSocket = reinterpret_cast<nx::network::AbstractDatagramSocket*>(soap->user);

        for (const auto& probeType: kProbeTypes)
        {
            QString guid = QnUuid::createUuid().toString();
            guid = QLatin1String("uuid:") + guid.mid(1, guid.length()-2);
            QByteArray data = QString(QLatin1String(STATIC_DISCOVERY_MESSAGE)).arg(guid).arg(probeType).toLatin1();
            qSocket->sendTo(data.data(), data.size(), WSDD_MULTICAST_ENDPOINT);
        }
        return SOAP_OK;
    }
}

OnvifResourceSearcherWsdd::ProbeContext::ProbeContext()
:
    soapWsddProxy( SOAP_IO_UDP )
{
}

OnvifResourceSearcherWsdd::ProbeContext::~ProbeContext()
{
}

void OnvifResourceSearcherWsdd::ProbeContext::initializeSoap()
{
    soapWsddProxy.soap->send_timeout = SOAP_DISCOVERY_TIMEOUT;
    soapWsddProxy.soap->recv_timeout = SOAP_DISCOVERY_TIMEOUT;
    soapWsddProxy.soap->user = sock.get();
    soapWsddProxy.soap->fconnect = nullGsoapFconnect;
    soapWsddProxy.soap->fdisconnect = nullGsoapFdisconnect;
    soapWsddProxy.soap->fsend = gsoapFsendSmall;
    soapWsddProxy.soap->frecv = gsoapFrecv;
    soapWsddProxy.soap->fopen = NULL;
    soapWsddProxy.soap->socket = -1;
    soapWsddProxy.soap->master = -1;
}

OnvifResourceSearcherWsdd::OnvifResourceSearcherWsdd(OnvifResourceInformationFetcher* informationFetcher):
    m_onvifFetcher(informationFetcher),
    m_shouldStop(false),
    m_isFirstSearch(true)
    /*,
    m_s
    m_recvSocketList(),
    m_mutex()*/
{
    //updateInterfacesListenSockets();
    qRegisterMetaType<OnvifResourceSearcherWsdd::ObtainMacFromMulticast>();
    QnJsonSerializer::registerSerializer<OnvifResourceSearcherWsdd::ObtainMacFromMulticast>();

}

OnvifResourceSearcherWsdd::~OnvifResourceSearcherWsdd()
{
    for( std::map<QString, ProbeContext*>::iterator
        it = m_ifaceToSock.begin();
        it != m_ifaceToSock.end();
        ++it )
    {
        delete it->second;
    }
    m_ifaceToSock.clear();
}

void OnvifResourceSearcherWsdd::pleaseStop()
{
    m_shouldStop = true;
    m_onvifFetcher->pleaseStop();
}

void OnvifResourceSearcherWsdd::findEndpoints(EndpointInfoHash& result)
{
    const QList<nx::network::QnInterfaceAndAddr>& intfList = nx::network::getAllIPv4Interfaces(
        nx::network::InterfaceListPolicy::keepAllAddressesPerInterface);

    if( !m_isFirstSearch )
    {
        for(const nx::network::QnInterfaceAndAddr& iface: intfList)
        {
            if (m_shouldStop)
                return;
            readProbeMatches( iface, result );
        }
        readHelloMessage(m_readMulticastContext.get(), result);
    }

    bool intfListChanged = (unsigned int)intfList.size() != m_ifaceToSock.size();
    if( !intfListChanged )
        for( const nx::network::QnInterfaceAndAddr& intf: intfList )
        {
            if( m_ifaceToSock.find( intf.address.toString() ) == m_ifaceToSock.end() )
                intfListChanged = true;
        }

    // if interface list is changed, remove old sockets
    if( intfListChanged )
    {
        std::map<QString, ProbeContext*>::iterator itr = m_ifaceToSock.begin();
        for(; itr != m_ifaceToSock.end() ; ++itr) {
            delete itr->second;
        }
        m_ifaceToSock.clear();
        createReadMulticastContext();
    }

    for(const nx::network::QnInterfaceAndAddr& iface: intfList)
    {
        if (m_shouldStop)
            return;
        sendProbe( iface );
    }

    if( m_isFirstSearch )
    {
        m_isFirstSearch = false;
#ifdef _WIN32
        ::Sleep( 1000 );
#else
        ::sleep( 1 );
#endif

        for(const nx::network::QnInterfaceAndAddr& iface: intfList)
        {
            if (m_shouldStop)
                return;
            readProbeMatches( iface, result );
        }
        readHelloMessage(m_readMulticastContext.get(), result);

        for(const nx::network::QnInterfaceAndAddr& iface: intfList)
        {
            if (m_shouldStop)
                return;
            sendProbe( iface );
        }
    }
}

void OnvifResourceSearcherWsdd::findResources(QnResourceList& result, DiscoveryMode discoveryMode)
{
    EndpointInfoHash endpoints;

    findEndpoints(endpoints);
    if (m_shouldStop)
        return;

    m_onvifFetcher->findResources(endpoints, result, discoveryMode);
}

QStringList OnvifResourceSearcherWsdd::getAddrPrefixes(const QString& host) const
{
    QStringList result;

    QStringList segments = host.split(QLatin1Char('.'));
    if (segments.size() != 4) {
        qCritical() << "OnvifResourceSearcherWsdd::getAddrPrefixes: is not IPv4 address: " << host;
        return result;
    }

    QString currPrefix = QLatin1String("http://") + segments[0] + QLatin1Char('.');
    result.push_front(currPrefix);

    currPrefix += segments[1] + QLatin1Char('.');
    result.push_front(currPrefix);

    currPrefix += segments[2] + QLatin1Char('.');
    result.push_front(currPrefix);

    return result;
}

template <class T>
QString OnvifResourceSearcherWsdd::getAppropriateAddress(const T* source, const QStringList& prefixes) const
{
    QString appropriateAddr;

    if (!source || !source->XAddrs)
        return appropriateAddr;

    int relevantLevel = 0;
    QString addrListStr = QLatin1String(source->XAddrs);
    const QStringList addrList = addrListStr.split(QLatin1Char(' '));
    if (!addrList.isEmpty() && prefixes.size() < 3)
        return addrList[0];

    for (const QString& addrStr: addrList) 
    {
        if (addrStr.startsWith(prefixes[2])) 
        {
            if (addrStr.startsWith(prefixes[0])) 
            {
                appropriateAddr = addrStr;
                break;
            } else if (addrStr.startsWith(prefixes[1])) 
            {
                appropriateAddr = addrStr;
                relevantLevel = 1;
            } else if (relevantLevel == 0) {
                appropriateAddr = addrStr;
            }
        }
    }

    return appropriateAddr;
}

template <class T>
QString OnvifResourceSearcherWsdd::getMac(const T* source, const SOAP_ENV__Header* header) const
{
    if (!source || !header) {
        return QString();
    }

    QString endpoint = QLatin1String(source->wsa5__EndpointReference.Address);
    QString messageId = QLatin1String(header->wsa5__MessageID);

    int pos = endpoint.lastIndexOf(QLatin1Char('-'));
    if (pos == -1) {
        return QString();
    }
    QString macFromEndpoint = endpoint.right(endpoint.size() - pos - 1).trimmed();

    pos = messageId.lastIndexOf(QLatin1Char('-'));
    if (pos == -1) {
        return QString();
    }
    QString macFromMessageId = messageId.right(messageId.size() - pos - 1).trimmed();

    static const int kMacAddressLength = 12;
    if (macFromEndpoint.size() == kMacAddressLength)
    {
        QString name = extractScope(source, QLatin1String(SCOPES_NAME_PREFIX));
        QString manufacturer = getManufacturer(source, name);
        auto dataPool = m_onvifFetcher->serverModule()->commonModule()->resourceDataPool();
        const QnResourceData resourceData = dataPool->data(manufacturer, name);
        ObtainMacFromMulticast obtainMacFromMulticast = ObtainMacFromMulticast::Auto;
        resourceData.value(OBTAIN_MAC_FROM_MULTICAST_PARAM_NAME, &obtainMacFromMulticast);

        if ((obtainMacFromMulticast == ObtainMacFromMulticast::Always)
            || (obtainMacFromMulticast == ObtainMacFromMulticast::Auto
                && macFromEndpoint == macFromMessageId))
        {
            QString result;
            for (int i = 1; i < kMacAddressLength; i += 2)
            {
                const int ind = i + i / 2;
                if (i < 11) result[ind + 1] = QLatin1Char('-');
                result[ind] = macFromEndpoint[i];
                result[ind - 1] = macFromEndpoint[i - 1];
            }

            return result.toUpper();
        }
    }

    return QString();
}

template <class T>
QString OnvifResourceSearcherWsdd::getEndpointAddress(const T* source) const
{
    if (!source) {
        return QString();
    }

    return QLatin1String(source->wsa5__EndpointReference.Address);
}

template <class T>
QString OnvifResourceSearcherWsdd::getManufacturer(const T* source, const QString& name) const
{
    if (!source || !source->Scopes || !source->Scopes->__item) {
        return QString();
    }

    QByteArray scopes = source->Scopes->__item;
    int posStart = scopes.indexOf(SCOPES_HARDWARE_PREFIX);
    if (posStart == -1) {
        return QString();
    }

    int posEnd = posStart != -1? scopes.indexOf(' ', posStart): -1;
    posEnd = posEnd != -1? posEnd: scopes.size();

    int skipSize = sizeof(SCOPES_HARDWARE_PREFIX) - 1;
    QByteArray percentEncodedValue = scopes.mid(posStart + skipSize, posEnd - posStart - skipSize).replace(name, "");
    QString result = QUrl::fromPercentEncoding(percentEncodedValue).trimmed();
    if (result.endsWith(lit("_")))
        result.chop(1);
    return result;
}

template<typename T>
std::set<QString> OnvifResourceSearcherWsdd::additionalManufacturers(
    const T* source,
    const std::vector<QString> additionalManufacturerPrefixes) const
{
    std::set<QString> result;
    for (const auto& prefix : additionalManufacturerPrefixes)
    {
        auto scopeValue = extractScope(source, prefix);
        if (!scopeValue.isEmpty())
            result.insert(scopeValue);
    }

    return result;
}

template <class T>
QString OnvifResourceSearcherWsdd::extractScope(const T* source, const QString& pattern) const
{
    if (!source || !source->Scopes || !source->Scopes->__item) {
            return QString();
    }

    QString scopes = QLatin1String(source->Scopes->__item);

    int posStart = scopes.indexOf(pattern);
    if (posStart == -1) {
        return QString();
    }

    int posEnd = posStart != -1? scopes.indexOf(QLatin1Char(' '), posStart): -1;
    posEnd = posEnd != -1? posEnd: scopes.size();

    int skipSize = pattern.length();
    QString percentEncodedValue = scopes.mid(posStart + skipSize, posEnd - posStart - skipSize);

    return QUrl::fromPercentEncoding(QByteArray(percentEncodedValue.toStdString().c_str())).trimmed();
}

void OnvifResourceSearcherWsdd::fillWsddStructs(wsdd__ProbeType& probe, wsa5__EndpointReferenceType& endpoint) const
{
    // Strings PROBE_TYPE and WSA_ADDRESS should not be changed
    // (possibly, char* instead of const char* is a gsoap bug
    // So const_cast should be safety

    probe.Scopes = NULL;

    endpoint.Address = const_cast<char*>(WSA_ADDRESS);

    endpoint.ReferenceParameters = nullptr;
    endpoint.Metadata = nullptr;

    endpoint.__size = 0;
    endpoint.__any = NULL;
    endpoint.__anyAttribute = NULL;
}

template <class T>
void OnvifResourceSearcherWsdd::addEndpointToHash(EndpointInfoHash& hash, const T* source,
    const SOAP_ENV__Header* header, const QStringList& addrPrefixes, const QString& host,
    bool macAddressRequired) const
{
    if (!source) {
        return;
    }

    QString endpointId = nx::utils::replaceNonFileNameCharacters(getEndpointAddress(source), QLatin1Char('_'));
    if (endpointId.isEmpty() || hash.contains(endpointId))
    {
        return;
    }

    const auto name = extractScope(source, QLatin1String(SCOPES_NAME_PREFIX));
    const auto manufacturer = getManufacturer(source, name);
    const auto location = extractScope(source, QLatin1String(SCOPES_LOCATION_PREFIX));
    const auto appropriateAddr = getAppropriateAddress(source, addrPrefixes);
    const auto additionalVendors = additionalManufacturers(
        source,
        kManufacturerScopePrefixes);

    QString mac = getMac(source, header);

    /*
     * Since version 4.1 we introduce to parse 'hello' messages in addition to regular 'probe' messages.
     * But some cameras fill some headers in 'probe' messages but keep it empty in 'hello' messages.
     * It causes some cameras have found twice in case of restart them, because 'mac' can be parsed
     * from 'probe' message, but can't be parsed from 'hello' message. The good fix is just to remove
     * trying to fill mac from discovery message, it not need any more. But we can't do it because
     * of compatibility with previous versions.
     */
    if (macAddressRequired && mac.isEmpty())
        return;

    QString uniqId = !mac.isEmpty() ? mac : endpointId;

    hash.insert(
        endpointId,
        EndpointAdditionalInfo(
            name,
            manufacturer,
            location,
            mac,
            uniqId,
            host,
            appropriateAddr,
            additionalVendors));
}

template <class T>
void OnvifResourceSearcherWsdd::printProbeMatches(const T* source, const SOAP_ENV__Header* header) const
{
    return;
    qDebug() << "OnvifResourceSearcherWsdd::printProbeMatches";

    qDebug() << "  Header: ";
    if (header) {

        if (header->wsa5__RelatesTo && header->wsa5__RelatesTo->__item) {
            qDebug() << "    Related to MessageID: " << header->wsa5__RelatesTo->__item;
        }

        if (header->wsa5__Action) {
            qDebug() << "    Action: " << header->wsa5__Action;
        }

        if (header->wsa5__To) {
            qDebug() << "    To: " << header->wsa5__To;
        }

        if (header->wsa5__From && header->wsa5__From->Address) {
            qDebug() << "    From: " << header->wsa5__From->Address;
        }

        if (header->wsa5__ReplyTo && header->wsa5__ReplyTo->Address) {
            qDebug() << "    ReplyTo: " << header->wsa5__ReplyTo->Address;
        }

        if (header->wsa5__FaultTo && header->wsa5__FaultTo->Address) {
            qDebug() << "    FaultTo: " << header->wsa5__FaultTo->Address;
        }

        if (header->wsa5__RelatesTo && header->wsa5__RelatesTo->__item) {
            qDebug() << "    FaultTo: " << header->wsa5__RelatesTo->__item;
        }

    } else {
        qDebug() << "    EMPTY";
    }

    qDebug() << "  Body: ";
    if (source) {

        if (source->MetadataVersion) {
            qDebug() << "    MetadataVersion: " << source->MetadataVersion;
        }

        if (source->Types) {
            qDebug() << "    Types: " << source->Types;
        }

        if (source->wsa5__EndpointReference.Address) {
            qDebug() << "    Address: " << source->wsa5__EndpointReference.Address;
        }

        if (source->Scopes && source->Scopes->__item) {
            qDebug() << "    Scopes: " << source->Scopes->__item;
        }

        if (source->XAddrs) {
            qDebug() << "    XAddrs: " << source->XAddrs;
        }

    } else {
        qDebug() << "    EMPTY";
    }
}

bool OnvifResourceSearcherWsdd::sendProbe( const nx::network::QnInterfaceAndAddr& iface )
{
    std::pair<std::map<QString, ProbeContext*>::iterator, bool> p = m_ifaceToSock.insert( std::make_pair( iface.address.toString(), (ProbeContext*)NULL ) );
    ProbeContext*& ctx = p.first->second;
    if( p.second )
    {
        ctx = new ProbeContext();
        ctx->sock = nx::network::SocketFactory::createDatagramSocket();
        //if( !ctx->sock->bindToInterface(iface) || !ctx->sock->setNonBlockingMode( true ) )
        if( !ctx->sock->bind(iface.address.toString(), 0) || !ctx->sock->setNonBlockingMode( true ) )
        {
            delete ctx;
            m_ifaceToSock.erase( p.first );
            return false;
        }
        ctx->sock->setMulticastIF(iface.address.toString());
    }

    ctx->initializeSoap();

    fillWsddStructs( ctx->wsddProbe, ctx->replyTo );

    char* messageID = const_cast<char*>(soap_wsa_rand_uuid(ctx->soapWsddProxy.soap));
    //qDebug() << "OnvifResourceSearcherWsdd::findEndpoints: MessageID: " << messageID << ". Interface: " << iface.address.toString();

    // Strings WSDD_ADDRESS and WSDD_ACTION should not be changed
    // (possibly,  char* instead of const char* in soap_header declaration, is a gsoap bug)
    // So const_cast should be safety
    ctx->soapWsddProxy.soap_header(
        messageID,
        /*wsa5__RelatesTo*/ nullptr,
        /*wsa5__From*/ nullptr,
        &ctx->replyTo,
        /*wsa5__FaultTo*/ nullptr,
        /*wsa5__To*/ const_cast<char*>(WSDD_ADDRESS),
        const_cast<char*>(WSDD_ACTION),
        /*chan__ChannelInstance*/ nullptr,
        /*wsdd__AppSequence*/ nullptr,
        /*wsse__Security*/ nullptr,
        /*subscriptionID*/ nullptr);

    QString targetAddr = QString::fromLatin1(WSDD_GSOAP_MULTICAST_ADDRESS);
    int soapRes = ctx->soapWsddProxy.send_Probe(targetAddr.toLatin1().data(), NULL, &ctx->wsddProbe);
    if( soapRes != SOAP_OK )
    {
        qWarning() << "OnvifResourceSearcherWsdd::findEndpoints: (Send) SOAP failed. GSoap error code: "
            << soapRes << SoapErrorHelper::fetchDescription(ctx->soapWsddProxy.soap_fault())
            << ". Interface: " << (iface.address.toString());
    }

    //ctx->soapWsddProxy.reset();
    soap_destroy(ctx->soapWsddProxy.soap);
    soap_end(ctx->soapWsddProxy.soap);

    if( soapRes == SOAP_OK )
        return true;

    //removing socket for it to be recreated on next sendProbe call
    delete ctx;
    m_ifaceToSock.erase( p.first );
    return false;
}

bool OnvifResourceSearcherWsdd::readProbeMatches( const nx::network::QnInterfaceAndAddr& iface, EndpointInfoHash& result )
{
    std::map<QString, ProbeContext*>::iterator it = m_ifaceToSock.find( iface.address.toString() );
    if( it == m_ifaceToSock.end() )
        return false;
    ProbeContext& ctx = *it->second;

    NX_ASSERT( ctx.soapWsddProxy.soap );

    //Receiving all ProbeMatches. Timeout = 500 ms, as written in ONVIF spec
    for( ;; )
    {
        if (m_shouldStop)
        {
            ctx.soapWsddProxy.reset();
            return false;
        }

        ctx.initializeSoap();

        __wsdd__ProbeMatches wsddProbeMatches;
        memset( &wsddProbeMatches, 0, sizeof(wsddProbeMatches) );

        int soapRes = ctx.soapWsddProxy.recv_ProbeMatches(wsddProbeMatches);
        if (soapRes != SOAP_OK)
        {
            if (soapRes == SOAP_EOF)
            {
                //qDebug() << "OnvifResourceSearcherWsdd::findEndpoints: All devices found. Interface: " << iface.address.toString();
            }
            else
            {
                qWarning() << "OnvifResourceSearcherWsdd::findEndpoints: SOAP failed. GSoap error code: "
                    << soapRes << SoapErrorHelper::fetchDescription(ctx.soapWsddProxy.soap_fault())
                    << ". Interface: " << (iface.address.toString());
            }
            ctx.soapWsddProxy.reset();
            //ctx.soapWsddProxy.destroy();
            //ctx.sock.reset();
            //delete it->second;
            //m_ifaceToSock.erase( it );
            return true;
        }

        if (wsddProbeMatches.wsdd__ProbeMatches)
        {
            addEndpointToHash(
                result,
                wsddProbeMatches.wsdd__ProbeMatches->ProbeMatch,
                ctx.soapWsddProxy.soap->header,
                getAddrPrefixes(iface.address.toString()),
                iface.address.toString());

            if (nx::utils::log::isToBeLogged(nx::utils::log::Level::debug))
            {
                printProbeMatches(wsddProbeMatches.wsdd__ProbeMatches->ProbeMatch,
                    ctx.soapWsddProxy.soap->header);
            }
        }

        ctx.soapWsddProxy.reset();
    }
}

bool OnvifResourceSearcherWsdd::createReadMulticastContext()
{
    const auto address = 
        nx::network::SocketAddress(nx::network::HostAddress::anyHost, WSDD_MULTICAST_PORT);
    m_readMulticastContext = std::make_unique<ProbeContext>();
    m_readMulticastContext->sock = nx::network::SocketFactory::createDatagramSocket();
    if (!m_readMulticastContext->sock->setReuseAddrFlag(true)
        || !m_readMulticastContext->sock->setReusePortFlag(true)
        || !m_readMulticastContext->sock->bind(address)
        || !m_readMulticastContext->sock->setNonBlockingMode(true))
    {
        NX_DEBUG(this, "Can not initialize WSDD socket to read 'hello' messages");
        m_readMulticastContext.reset();
        return false;
    }

    const QList<nx::network::QnInterfaceAndAddr>& intfList = nx::network::getAllIPv4Interfaces(
        nx::network::InterfaceListPolicy::keepAllAddressesPerInterface);
    for (const nx::network::QnInterfaceAndAddr& iface: intfList)
        m_readMulticastContext->sock->joinGroup(WSDD_MULTICAST_ADDRESS, iface.address.toString());
    m_readMulticastContext->initializeSoap();

    return true;
}

void OnvifResourceSearcherWsdd::readHelloMessage(ProbeContext* ctx, EndpointInfoHash& result)
{
    if (!ctx)
        return;
    if (!NX_ASSERT(ctx->soapWsddProxy.soap))
        return;

    ctx->initializeSoap();

    int soapRes = SOAP_OK;
    while (soapRes == SOAP_OK && !m_shouldStop)
    {
        __wsdd__Hello hello;
        memset(&hello, 0, sizeof(hello));
        soapRes = ctx->soapWsddProxy.recv_Hello(hello);
        if (soapRes != SOAP_OK && soapRes != SOAP_EOF)
        {
            NX_WARNING(this, "OnvifResourceSearcherWsdd::findEndpoints: SOAP failed. GSoap error code: %1(%2)",
                soapRes, SoapErrorHelper::fetchDescription(ctx->soapWsddProxy.soap_fault()));
            break;
        }

        if (hello.wsdd__Hello)
        {
            addEndpointToHash(
                result,
                hello.wsdd__Hello,
                ctx->soapWsddProxy.soap->header,
                QStringList(),
                QString(),
                /*macAddressRequired*/ true);

            if (nx::utils::log::isToBeLogged(nx::utils::log::Level::debug))
            {
                printProbeMatches(hello.wsdd__Hello,
                    ctx->soapWsddProxy.soap->header);
            }
        }
    }
    ctx->soapWsddProxy.reset();
}

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(OnvifResourceSearcherWsdd, ObtainMacFromMulticast,
(OnvifResourceSearcherWsdd::Auto, "Auto")
(OnvifResourceSearcherWsdd::Always, "Always")
(OnvifResourceSearcherWsdd::Never, "Never"))

#endif //ENABLE_ONVIF
