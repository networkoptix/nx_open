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
#include <common/static_common_module.h>
#include <core/resource/resource_data.h>
#include <core/resource_management/resource_data_pool.h>


//static const int SOAP_DISCOVERY_TIMEOUT = 1; // "+" in seconds, "-" in mseconds
static const int SOAP_DISCOVERY_TIMEOUT = -500; // "+" in seconds, "-" in mseconds
static const int SOAP_HELLO_CHECK_TIMEOUT = -1; // "+" in seconds, "-" in mseconds
static const int CHECK_HELLO_RETRY_COUNT = 50;

//extern bool multicastJoinGroup(QUdpSocket& udpSocket, QHostAddress groupAddress, QHostAddress localAddress);
//extern bool multicastLeaveGroup(QUdpSocket& udpSocket, QHostAddress groupAddress);

QString OnvifResourceSearcherWsdd::LOCAL_ADDR(QLatin1String("127.0.0.1"));
const char OnvifResourceSearcherWsdd::SCOPES_NAME_PREFIX[] = "onvif://www.onvif.org/name/";
const char OnvifResourceSearcherWsdd::SCOPES_HARDWARE_PREFIX[] = "onvif://www.onvif.org/hardware/";
const char OnvifResourceSearcherWsdd::SCOPES_LOCATION_PREFIX[] = "onvif://www.onvif.org/location/";
const char OnvifResourceSearcherWsdd::PROBE_TYPE[] = "onvifDiscovery:NetworkVideoTransmitter";
const char OnvifResourceSearcherWsdd::WSA_ADDRESS[] = "http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous";
const char OnvifResourceSearcherWsdd::WSDD_ADDRESS[] = "urn:schemas-xmlsoap-org:ws:2005:04:discovery";
const char OnvifResourceSearcherWsdd::WSDD_ACTION[] = "http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe";

static const std::vector<QString> kManufacturerScopePrefixes
    = {lit("onvif://www.onvif.org/manufacturer/")};

const char OnvifResourceSearcherWsdd::WSDD_GSOAP_MULTICAST_ADDRESS[] = "soap.udp://239.255.255.250:3702";

static const int WSDD_MULTICAST_PORT = 3702;
static const char WSDD_MULTICAST_ADDRESS[] = "239.255.255.250";
static const nx::network::SocketAddress WSDD_MULTICAST_ENDPOINT( WSDD_MULTICAST_ADDRESS, WSDD_MULTICAST_PORT );


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
#if 0
    //Socket send through UdpSocket
    int gsoapFsend(struct soap *soap, const char *s, size_t n)
    {
        nx::network::AbstractDatagramSocket* qSocket = reinterpret_cast<nx::network::AbstractDatagramSocket*>(soap->user);
        qSocket->sendTo(s, static_cast<unsigned int>(n), WSDD_MULTICAST_ENDPOINT);
        return SOAP_OK;
    }
#endif
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
<d:Types xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" xmlns:dp0=\"http://www.onvif.org/ver10/network/wsdl\">dp0:NetworkVideoTransmitter</d:Types>\
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

        QString guid = QnUuid::createUuid().toString();
        guid = QLatin1String("uuid:") + guid.mid(1, guid.length()-2);
        QByteArray data = QString(QLatin1String(STATIC_DISCOVERY_MESSAGE)).arg(guid).toLatin1();

        qSocket->sendTo(data.data(), data.size(), WSDD_MULTICAST_ENDPOINT);
        return SOAP_OK;
    }
#if 0
    int gsoapFsendSmallUnicast(struct soap *soap, const char *s, size_t n)
    {
        //avoiding sending numerous data
        if (!QByteArray::fromRawData(s, static_cast<int>(n)).startsWith("<?xml")) {
            return SOAP_OK;
        }

        QString msgId;
        nx::network::AbstractDatagramSocket* socket = reinterpret_cast<nx::network::AbstractDatagramSocket*>(soap->user);

        QString guid = QnUuid::createUuid().toString();
        guid = QLatin1String("uuid:") + guid.mid(1, guid.length()-2);
        QByteArray data = QString(QLatin1String(STATIC_DISCOVERY_MESSAGE)).arg(guid).toLatin1();

        //socket.connectToHost(QHostAddress(QString::fromLatin1(soap->host)), WSDD_MULTICAST_PORT);
        //socket.write(data);
        socket->sendTo(data.data(), data.size(), nx::network::SocketAddress( soap->host, WSDD_MULTICAST_PORT ) );
        return SOAP_OK;
    }
#endif
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

/*void OnvifResourceSearcherWsdd::updateInterfacesListenSockets() const
{
    QnMutexLocker lock( &m_mutex );

    QList<nx::network::QnInterfaceAndAddr> interfaces = nx::network::getAllIPv4Interfaces();

    //Remove outdated
    QHash<QString, QUdpSocketPtr>::iterator it = m_recvSocketList.begin();
    while (it != m_recvSocketList.end()) {
        bool found = false;
        for(const nx::network::QnInterfaceAndAddr& iface: interfaces)
        {
            if (m_recvSocketList.contains(iface.address.toString())) {
                found = true;
                break;
            }
        }

        if (!found) {
            multicastLeaveGroup(*(it.value()), WSDD_GROUP_ADDRESS);
            it = m_recvSocketList.erase(it);
        } else {
            ++it;
        }
    }

    //Add new
    for(const nx::network::QnInterfaceAndAddr& iface: interfaces)
    {
        QString key = iface.address.toString();

        if (m_recvSocketList.contains(key))
            continue;

        QUdpSocketPtr socket(new QUdpSocket());
        bool bindSucceeded = socket->bind(QHostAddress::Any, WSDD_MULTICAST_PORT,
            QUdpSocket::ReuseAddressHint | QUdpSocket::ShareAddress);

        if (!bindSucceeded) {
            qWarning() << "OnvifResourceSearcherWsdd::updateInterfacesListenSockets: faild to bind. Address: " << key;
            continue;
        }

        if (!multicastJoinGroup(*socket, WSDD_GROUP_ADDRESS, iface.address)) {
            qWarning() << "OnvifResourceSearcherWsdd::updateInterfacesListenSockets: multicast join group failed. Address: " << key;
            continue;
        }

        m_recvSocketList.insert(key, socket);
    }
}

void OnvifResourceSearcherWsdd::findHelloEndpoints(EndpointInfoHash& result) const
{
    QnMutexLocker lock( &m_mutex );

    wsddProxy soapWsddProxy(SOAP_IO_UDP);
    soapWsddProxy.soap->send_timeout = SOAP_HELLO_CHECK_TIMEOUT;
    soapWsddProxy.soap->recv_timeout = SOAP_HELLO_CHECK_TIMEOUT;
    soapWsddProxy.soap->connect_timeout = SOAP_HELLO_CHECK_TIMEOUT;
    soapWsddProxy.soap->accept_timeout = SOAP_HELLO_CHECK_TIMEOUT;
    soapWsddProxy.soap->fconnect = nullGsoapFconnect;
    soapWsddProxy.soap->fdisconnect = nullGsoapFdisconnect;
    soapWsddProxy.soap->fclose = NULL;
    soapWsddProxy.soap->fopen = NULL;

    QHash<QString, QUdpSocketPtr>::iterator it = m_recvSocketList.begin();
    for (; it != m_recvSocketList.end(); ++it)
    {
        QStringList addrPrefixes = getAddrPrefixes(it.key());
        QUdpSocket& socket = *(it.value().data());

        soapWsddProxy.soap->socket = socket.socketDescriptor();
        soapWsddProxy.soap->master = socket.socketDescriptor();

        //Receiving all ProbeMatches
        for (int i = 0; i < CHECK_HELLO_RETRY_COUNT; ++i)
        {
            __wsdd__Hello wsddHello;
            wsddHello.wsdd__Hello = NULL;

            int soapRes = soapWsddProxy.recv_Hello(wsddHello);
            if (soapRes != SOAP_OK)
            {
                if (soapRes == SOAP_EOF || SOAP_NO_METHOD)
                {
                    qDebug() << "OnvifResourceSearcherWsdd::findHelloEndpoints: All devices found. Interface: " << it.key();
                    soap_destroy(soapWsddProxy.soap);
                    soap_end(soapWsddProxy.soap);
                    break;
                }
                else
                {
                    //SOAP_NO_METHOD - The dispatcher did not find a matching operation for the request
                    //So, this is not error, silently ignore
                    qWarning() << "OnvifResourceSearcherWsdd::findHelloEndpoints: SOAP failed. GSoap error code: "
                        << soapRes << SoapErrorHelper::fetchDescription(soapWsddProxy.soap_fault())
                        << ". Interface: " << it.key();

                    soap_destroy(soapWsddProxy.soap);
                    soap_end(soapWsddProxy.soap);
                    continue;
                }
            }

            addEndpointToHash(result, wsddHello.wsdd__Hello, soapWsddProxy.soap->header, addrPrefixes, it.key());

            if (cl_log.logLevel() >= cl_logDEBUG1) {
                printProbeMatches(wsddHello.wsdd__Hello, soapWsddProxy.soap->header);
            }

            soap_destroy(soapWsddProxy.soap);
            soap_end(soapWsddProxy.soap);
        }

        soapWsddProxy.soap->socket = SOAP_INVALID_SOCKET;
        soapWsddProxy.soap->master = SOAP_INVALID_SOCKET;
    }
}*/

void OnvifResourceSearcherWsdd::findEndpoints(EndpointInfoHash& result)
{
    const QList<nx::network::QnInterfaceAndAddr>& intfList = nx::network::getAllIPv4Interfaces();

    if( !m_isFirstSearch )
    {
        for(const nx::network::QnInterfaceAndAddr& iface: intfList)
        {
            if (m_shouldStop)
                return;
            readProbeMatches( iface, result );
        }
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
            readProbeMatches( iface, result );

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

    //for( int i = 0; i < 100; ++i )
    //{
        findEndpoints(endpoints);
    //    endpoints.clear();
    //}
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

    if (!source || !source->XAddrs) {
        return appropriateAddr;
    }

    int relevantLevel = 0;
    QString addrListStr = QLatin1String(source->XAddrs);
    QStringList addrList = addrListStr.split(QLatin1Char(' '));
    for (const QString& addrStr: addrList) {
        if (addrStr.startsWith(prefixes[2])) {
            if (addrStr.startsWith(prefixes[0])) {
                appropriateAddr = addrStr;
                break;
            } else if (addrStr.startsWith(prefixes[1])) {
                appropriateAddr = addrStr;
                relevantLevel = 1;
            } else if (relevantLevel == 0) {
                appropriateAddr = addrStr;
            }
        }
    }

    //qDebug() << "OnvifResourceSearcherWsdd::getAppropriateAddress: address = " << appropriateAddr
    //         << ". Interface: " << prefixes[0];

    return appropriateAddr;
}

template <class T>
QString OnvifResourceSearcherWsdd::getMac(const T* source, const SOAP_ENV__Header* header) const
{
    if (!source || !header) {
        return QString();
    }

    QString endpoint = QLatin1String(source->wsa__EndpointReference.Address);
    QString messageId = QLatin1String(header->wsa__MessageID);

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

        const QnResourceData resourceData = qnStaticCommon->dataPool()->data(manufacturer, name);

        if (macFromEndpoint == macFromMessageId
            || resourceData.value<bool>(Qn::MAC_FROM_MULTICAST_PARAM_NAME))
        {
            QString result;
            for (int i = 1; i < kMacAddressLength; i += 2)
            {
                int ind = i + i / 2;
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

    return QLatin1String(source->wsa__EndpointReference.Address);
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

void OnvifResourceSearcherWsdd::fillWsddStructs(wsdd__ProbeType& probe, wsa__EndpointReferenceType& endpoint) const
{
    //String should not be changed (possibly, declaration of char* instead of const char*,- gsoap bug
    //So const_cast should be safety

    probe.Types = const_cast<char*>(PROBE_TYPE);
    probe.Scopes = NULL;

    endpoint.Address = const_cast<char*>(WSA_ADDRESS);
    endpoint.PortType = NULL;
    endpoint.ReferenceParameters = NULL;
    endpoint.ReferenceProperties = NULL;
    endpoint.ServiceName = NULL;
    endpoint.__size = 0;
    endpoint.__any = NULL;
    endpoint.__anyAttribute = NULL;
}

template <class T>
void OnvifResourceSearcherWsdd::addEndpointToHash(EndpointInfoHash& hash, const T* source,
    const SOAP_ENV__Header* header, const QStringList& addrPrefixes, const QString& host) const
{
    if (!source) {
        return;
    }

    QString appropriateAddr = getAppropriateAddress(source, addrPrefixes);
    if (appropriateAddr.isEmpty() || hash.contains(appropriateAddr)) {
        return;
    }

    const auto name = extractScope(source, QLatin1String(SCOPES_NAME_PREFIX));
    const auto manufacturer = getManufacturer(source, name);
    const auto location = extractScope(source, QLatin1String(SCOPES_LOCATION_PREFIX));
    const auto additionalVendors = additionalManufacturers(
        source,
        kManufacturerScopePrefixes);

    QString mac = getMac(source, header);

    QString endpointId = nx::utils::replaceNonFileNameCharacters(getEndpointAddress(source), QLatin1Char('_'));
    QString uniqId = !mac.isEmpty() ? mac : endpointId;

    hash.insert(
        appropriateAddr,
        EndpointAdditionalInfo(
            name,
            manufacturer,
            location,
            mac,
            uniqId,
            host,
            additionalVendors));
}

template <class T>
void OnvifResourceSearcherWsdd::printProbeMatches(const T* source, const SOAP_ENV__Header* header) const
{
    return;
    qDebug() << "OnvifResourceSearcherWsdd::printProbeMatches";

    qDebug() << "  Header: ";
    if (header) {

        if (header->wsa__RelatesTo && header->wsa__RelatesTo->__item) {
            qDebug() << "    Related to MessageID: " << header->wsa__RelatesTo->__item;
        }

        if (header->wsa__Action) {
            qDebug() << "    Action: " << header->wsa__Action;
        }

        if (header->wsa__To) {
            qDebug() << "    To: " << header->wsa__To;
        }

        if (header->wsa__From && header->wsa__From->Address) {
            qDebug() << "    From: " << header->wsa__From->Address;
        }

        if (header->wsa__ReplyTo && header->wsa__ReplyTo->Address) {
            qDebug() << "    ReplyTo: " << header->wsa__ReplyTo->Address;
        }

        if (header->wsa__FaultTo && header->wsa__FaultTo->Address) {
            qDebug() << "    FaultTo: " << header->wsa__FaultTo->Address;
        }

        if (header->wsa__RelatesTo && header->wsa__RelatesTo->__item) {
            qDebug() << "    FaultTo: " << header->wsa__RelatesTo->__item;
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

        if (source->wsa__EndpointReference.Address) {
            qDebug() << "    Address: " << source->wsa__EndpointReference.Address;
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

#if 0
void OnvifResourceSearcherWsdd::findEndpointsImpl( EndpointInfoHash& result, const nx::network::QnInterfaceAndAddr& iface ) const
{
    if( m_shouldStop )
        return;

    std::pair<std::map<QString, UDPSocket*>::iterator, bool> p = m_ifaceToSock.insert( std::make_pair( iface.address.toString(), (UDPSocket*)NULL ) );
    if( p.second )
    {
        p.first->second. = new UDPSocket(AF_INET);
        if( !p.first->second->bindToInterface(iface) || !p.first->second->setNonBlockingMode( true ) )
        {
            delete p.first->second;
            m_ifaceToSock.erase( p.first );
            return;
        }
    }
    UDPSocket* socket = p.first->second;

    //UDPSocket socket;
    //if (!socket.bindToInterface(iface))
    //    return;
    //socket.setRecvTimeout(SOAP_DISCOVERY_TIMEOUT * 1000);

    QStringList addrPrefixes = getAddrPrefixes(iface.address.toString());
    wsddProxy soapWsddProxy(SOAP_IO_UDP);
    soapWsddProxy.soap->send_timeout = SOAP_DISCOVERY_TIMEOUT;
    soapWsddProxy.soap->recv_timeout = SOAP_DISCOVERY_TIMEOUT;
    //soapWsddProxy.soap->connect_timeout = SOAP_DISCOVERY_TIMEOUT;
    //soapWsddProxy.soap->accept_timeout = SOAP_DISCOVERY_TIMEOUT;
    soapWsddProxy.soap->user = socket;
    soapWsddProxy.soap->fconnect = nullGsoapFconnect;
    soapWsddProxy.soap->fdisconnect = nullGsoapFdisconnect;
    soapWsddProxy.soap->fsend = gsoapFsendSmall;
    soapWsddProxy.soap->frecv = gsoapFrecv;
    soapWsddProxy.soap->fopen = NULL;
    soapWsddProxy.soap->socket = socket->handle();
    soapWsddProxy.soap->master = socket->handle();

    wsdd__ProbeType wsddProbe;
    wsa__EndpointReferenceType replyTo;
    fillWsddStructs(wsddProbe, replyTo);

    char* messageID = const_cast<char*>(soap_wsa_rand_uuid(soapWsddProxy.soap));
    //qDebug() << "OnvifResourceSearcherWsdd::findEndpoints: MessageID: " << messageID << ". Interface: " << iface.address.toString();

    //String should not be changed (possibly, declaration of char* instead of const char*,- gsoap bug
    //So const_cast should be safety
    soapWsddProxy.soap_header(NULL, messageID, NULL, NULL, &replyTo, NULL,
        const_cast<char*>(WSDD_ADDRESS), const_cast<char*>(WSDD_ACTION), NULL);

    QString targetAddr = QString::fromLatin1(WSDD_GSOAP_MULTICAST_ADDRESS);
    int soapRes = soapWsddProxy.send_Probe(targetAddr.toLatin1().data(), NULL, &wsddProbe);
    soapRes = soapWsddProxy.send_Probe(targetAddr.toLatin1().data(), NULL, &wsddProbe);

    if (soapRes != SOAP_OK)
    {
        qWarning() << "OnvifResourceSearcherWsdd::findEndpoints: (Send) SOAP failed. GSoap error code: "
            << soapRes << SoapErrorHelper::fetchDescription(soapWsddProxy.soap_fault())
            << ". Interface: " << (iface.address.toString());
        soap_destroy(soapWsddProxy.soap);
        soap_end(soapWsddProxy.soap);
        return;
    }

    soap_destroy(soapWsddProxy.soap);
    soap_end(soapWsddProxy.soap);

    //Receiving all ProbeMatches. Timeout = 500 ms, as written in ONVIF spec
    while (true)
    {
        if (m_shouldStop)
            return;

        __wsdd__ProbeMatches wsddProbeMatches;
        wsddProbeMatches.wsdd__ProbeMatches = NULL;

        soapRes = soapWsddProxy.recv_ProbeMatches(wsddProbeMatches);
        if (soapRes != SOAP_OK)
        {
            if (soapRes == SOAP_EOF)
            {
                //qDebug() << "OnvifResourceSearcherWsdd::findEndpoints: All devices found. Interface: " << iface.address.toString();
            }
            else
            {
                qWarning() << "OnvifResourceSearcherWsdd::findEndpoints: SOAP failed. GSoap error code: "
                    << soapRes << SoapErrorHelper::fetchDescription(soapWsddProxy.soap_fault())
                    << ". Interface: " << (iface.address.toString());
            }
            soap_destroy(soapWsddProxy.soap);
            soap_end(soapWsddProxy.soap);
            break;
        }

        if (wsddProbeMatches.wsdd__ProbeMatches)
        {
            addEndpointToHash(result, wsddProbeMatches.wsdd__ProbeMatches->ProbeMatch, soapWsddProxy.soap->header, addrPrefixes,
                iface.address.toString());

            if (cl_log.logLevel() >= cl_logDEBUG1)
            {
                printProbeMatches(wsddProbeMatches.wsdd__ProbeMatches->ProbeMatch, soapWsddProxy.soap->header);
            }
        }

        soap_destroy(soapWsddProxy.soap);
        soap_end(soapWsddProxy.soap);
    }

    soapWsddProxy.soap->socket = SOAP_INVALID_SOCKET;
    soapWsddProxy.soap->master = SOAP_INVALID_SOCKET;
    //socket.close();
}
#endif

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

    //String should not be changed (possibly, declaration of char* instead of const char*,- gsoap bug
    //So const_cast should be safety
    ctx->soapWsddProxy.soap_header(NULL, messageID, NULL, NULL, &ctx->replyTo, NULL,
        const_cast<char*>(WSDD_ADDRESS), const_cast<char*>(WSDD_ACTION), NULL);

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

#endif //ENABLE_ONVIF
