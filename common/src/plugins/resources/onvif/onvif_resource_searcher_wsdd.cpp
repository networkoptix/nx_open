#include "openssl/evp.h"

#include <quuid.h>

#include "onvif_resource_searcher_wsdd.h"
#include "core/resource/camera_resource.h"
#include "onvif_resource.h"
#include "onvif_helper.h"
//#include "onvif/Onvif.nsmap"
#include "onvif/wsaapi.h"

//#include "onvif_ws_searcher_helper.h"
//#include "../digitalwatchdog/digital_watchdog_resource.h"
//#include "../brickcom/brickcom_resource.h"
//#include "../sony/sony_resource.h"

#include "utils/network/nettools.h"
//#include "utils/common/sleep.h"
//#include "utils/network/mac_address.h"
//#include "../digitalwatchdog/digital_watchdog_resource.h"
//#include "../brickcom/brickcom_resource.h"
//#include "utils/common/rand.h"
//#include "../sony/sony_resource.h"
#include "utils/common/string.h"

//static const int SOAP_DISCOVERY_TIMEOUT = 1; // "+" in seconds, "-" in mseconds
static const int SOAP_DISCOVERY_TIMEOUT = 0; // "+" in seconds, "-" in mseconds
static const int SOAP_HELLO_CHECK_TIMEOUT = -1; // "+" in seconds, "-" in mseconds
static const int CHECK_HELLO_RETRY_COUNT = 50;

//extern bool multicastJoinGroup(QUdpSocket& udpSocket, QHostAddress groupAddress, QHostAddress localAddress);
//extern bool multicastLeaveGroup(QUdpSocket& udpSocket, QHostAddress groupAddress);

QString& OnvifResourceSearcherWsdd::LOCAL_ADDR = *new QString(QLatin1String("127.0.0.1"));
const char OnvifResourceSearcherWsdd::SCOPES_NAME_PREFIX[] = "onvif://www.onvif.org/name/";
const char OnvifResourceSearcherWsdd::SCOPES_HARDWARE_PREFIX[] = "onvif://www.onvif.org/hardware/";
const char OnvifResourceSearcherWsdd::PROBE_TYPE[] = "onvifDiscovery:NetworkVideoTransmitter";
const char OnvifResourceSearcherWsdd::WSA_ADDRESS[] = "http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous";
const char OnvifResourceSearcherWsdd::WSDD_ADDRESS[] = "urn:schemas-xmlsoap-org:ws:2005:04:discovery";
const char OnvifResourceSearcherWsdd::WSDD_ACTION[] = "http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe";

const char OnvifResourceSearcherWsdd::WSDD_GSOAP_MULTICAST_ADDRESS[] = "soap.udp://239.255.255.250:3702";

int WSDD_MULTICAST_PORT = 3702;
const char WSDD_MULTICAST_ADDRESS[] = "239.255.255.250";

#define WSDD_GROUP_ADDRESS QHostAddress(QLatin1String(WSDD_MULTICAST_ADDRESS))


OnvifResourceSearcherWsdd::OnvifResourceSearcherWsdd():
    m_onvifFetcher(OnvifResourceInformationFetcher::instance()),
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
        delete it->second->sock;
        delete it->second;
    }
    m_ifaceToSock.clear();
}

OnvifResourceSearcherWsdd& OnvifResourceSearcherWsdd::instance()
{
    static OnvifResourceSearcherWsdd inst;
    return inst;
}

void OnvifResourceSearcherWsdd::pleaseStop()
{
    m_shouldStop = true;
    m_onvifFetcher.pleaseStop();
}

//To avoid recreating of gsoap socket, these 2 functions must be assigned
int nullGsoapFconnect(struct soap*, const char*, const char*, int)
{
    return SOAP_OK;
}

int nullGsoapFdisconnect(struct soap*)
{
    return SOAP_OK;
}

//Socket send through UdpSocket
int gsoapFsend(struct soap *soap, const char *s, size_t n)
{
    UDPSocket& qSocket = *reinterpret_cast<UDPSocket*>(soap->user);
    qSocket.sendTo(s, n, lit(WSDD_MULTICAST_ADDRESS), WSDD_MULTICAST_PORT);
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
<d:Types xmlns:d=\"http://schemas.xmlsoap.org/ws/2005/04/discovery\" xmlns:dp0=\"http://www.onvif.org/ver10/network/wsdl\">dp0:NetworkVideoTransmitter</d:Types>\
</Probe>\
</s:Body>\
</s:Envelope>";


// avoid SOAP select call
size_t gsoapFrecv(struct soap* soap, char* data, size_t maxSize)
{
    UDPSocket* qSocket = reinterpret_cast<UDPSocket*>(soap->user);
    int readed = qSocket->recv(data, maxSize);
    return (size_t) qMax(0, readed);
}


//Socket send through UDPSocket
int gsoapFsendSmall(struct soap *soap, const char *s, size_t n)
{
    QString recvData = QString::fromLatin1(QByteArray(s, (int) n).data());
    //avoiding sending numerous data
    if (!recvData.startsWith(QString::fromLatin1("<?xml"))) {
        return SOAP_OK;
    }

    Q_UNUSED(s)
    Q_UNUSED(n)
    QString msgId;
    UDPSocket& qSocket = *reinterpret_cast<UDPSocket*>(soap->user);

    QString guid = QUuid::createUuid().toString();
    guid = QLatin1String("uuid:") + guid.mid(1, guid.length()-2);
    QByteArray data = QString(QLatin1String(STATIC_DISCOVERY_MESSAGE)).arg(guid).toLocal8Bit();

    qSocket.sendTo(data.data(), data.size(), lit(WSDD_MULTICAST_ADDRESS), WSDD_MULTICAST_PORT);
    return SOAP_OK;
}

int gsoapFsendSmallUnicast(struct soap *soap, const char *s, size_t n)
{
    QString recvData = QString::fromLatin1(QByteArray(s, (int) n).data());
    //avoiding sending numerous data
    if (!recvData.startsWith(QString::fromLatin1("<?xml"))) {
        return SOAP_OK;
    }

    Q_UNUSED(s)
    Q_UNUSED(n)
    QString msgId;
    UDPSocket& socket = *reinterpret_cast<UDPSocket*>(soap->user);

    QString guid = QUuid::createUuid().toString();
    guid = QLatin1String("uuid:") + guid.mid(1, guid.length()-2);
    QByteArray data = QString(QLatin1String(STATIC_DISCOVERY_MESSAGE)).arg(guid).toLocal8Bit();

    //socket.connectToHost(QHostAddress(QString::fromLatin1(soap->host)), WSDD_MULTICAST_PORT);
    //socket.write(data);
    socket.sendTo(data.data(), data.size(), QString::fromLatin1(soap->host), WSDD_MULTICAST_PORT);
    return SOAP_OK;
}


/*void OnvifResourceSearcherWsdd::updateInterfacesListenSockets() const
{
    QMutexLocker lock(&m_mutex);

    QList<QnInterfaceAndAddr> interfaces = getAllIPv4Interfaces();

    //Remove outdated
    QHash<QString, QUdpSocketPtr>::iterator it = m_recvSocketList.begin();
    while (it != m_recvSocketList.end()) {
        bool found = false;
        foreach(QnInterfaceAndAddr iface, interfaces)
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
    foreach(QnInterfaceAndAddr iface, interfaces)
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
    QMutexLocker lock(&m_mutex);

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
#if 0
    foreach(QnInterfaceAndAddr iface, getAllIPv4Interfaces())
    {
        if (m_shouldStop)
            return;

        findEndpointsImpl( result, iface );
    }
#else
    const QList<QnInterfaceAndAddr>& intfList = getAllIPv4Interfaces();
    foreach(QnInterfaceAndAddr iface, intfList)
    {
        if (m_shouldStop)
            return;

        if( !sendProbe( iface ) )
            continue;
    }

    if( m_isFirstSearch )
    {
        m_isFirstSearch = false;
#ifdef _WIN32
        ::Sleep( 1000 );
#else
        ::sleep( 1 );
#endif
    }

    foreach(QnInterfaceAndAddr iface, intfList)
    {
        readProbeMatches( iface, result );
    }
#endif
}

void OnvifResourceSearcherWsdd::findResources(QnResourceList& result)
{
    EndpointInfoHash endpoints;

    //updateInterfacesListenSockets();
    //findHelloEndpoints(endpoints);


    findEndpoints(endpoints);

#if 0
    //ToDo: delete
    EndpointInfoHash::ConstIterator endpIter = endpoints.begin();
    qDebug() << "OnvifResourceSearcherWsdd::findResources: Endpoints in the list:"
             << (endpoints.size()? "": " EMPTY");

    while (endpIter != endpoints.end()) 
    {
        qDebug() << "    " << endpIter.key() << " (" << endpIter.value().uniqId << "): " << endpIter.value().manufacturer
                 << " - " << endpIter.value().name << ", discovered in " << endpIter.value().discoveryIp;
        ++endpIter;
    }
#endif
    m_onvifFetcher.findResources(endpoints, result);
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
    foreach (const QString addrStr, addrList) {
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

    if (macFromEndpoint.size() == 12 && macFromEndpoint == macFromMessageId) {
        QString result;
        for (int i = 1; i < 12; i += 2) {
            int ind = i + i / 2;
            if (i < 11) result[ind + 1] = QLatin1Char('-');
            result[ind] = macFromEndpoint[i];
            result[ind - 1] = macFromEndpoint[i - 1];
        }

        return result.toUpper();
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
    int posStart = scopes.indexOf(SCOPES_NAME_PREFIX);
    if (posStart == -1) {
        return QString();
    }

    int posEnd = posStart != -1? scopes.indexOf(' ', posStart): -1;
    posEnd = posEnd != -1? posEnd: scopes.size();

    int skipSize = sizeof(SCOPES_NAME_PREFIX) - 1;
    QByteArray percentEncodedValue = scopes.mid(posStart + skipSize, posEnd - posStart - skipSize).replace(name, "");

    return QUrl::fromPercentEncoding(percentEncodedValue).trimmed();
}

template <class T>
QString OnvifResourceSearcherWsdd::getName(const T* source) const
{
    if (!source || !source->Scopes || !source->Scopes->__item) {
            return QString();
    }

    QString scopes = QLatin1String(source->Scopes->__item);


    int posStart = scopes.indexOf(QLatin1String(SCOPES_HARDWARE_PREFIX));
    if (posStart == -1) {
        return QString();
    }

    int posEnd = posStart != -1? scopes.indexOf(QLatin1Char(' '), posStart): -1;
    posEnd = posEnd != -1? posEnd: scopes.size();

    int skipSize = sizeof(SCOPES_HARDWARE_PREFIX) - 1;
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

    QString name = getName(source);
    QString manufacturer = getManufacturer(source, name);
    QString mac = getMac(source, header);

    QString endpointId = replaceNonFileNameCharacters(getEndpointAddress(source), QLatin1Char('_'));
    QString uniqId = !mac.isEmpty() ? mac : endpointId;

    hash.insert(appropriateAddr, EndpointAdditionalInfo(name, manufacturer, mac, uniqId, host));
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
void OnvifResourceSearcherWsdd::findEndpointsImpl( EndpointInfoHash& result, const QnInterfaceAndAddr& iface ) const
{
    if( m_shouldStop )
        return;

    std::pair<std::map<QString, UDPSocket*>::iterator, bool> p = m_ifaceToSock.insert( std::make_pair( iface.address.toString(), (UDPSocket*)NULL ) );
    if( p.second )
    {
        p.first->second. = new UDPSocket();
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
    //socket.setReadTimeOut(SOAP_DISCOVERY_TIMEOUT * 1000);

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

bool OnvifResourceSearcherWsdd::sendProbe( const QnInterfaceAndAddr& iface )
{
    std::pair<std::map<QString, ProbeContext*>::iterator, bool> p = m_ifaceToSock.insert( std::make_pair( iface.address.toString(), (ProbeContext*)NULL ) );
    ProbeContext*& ctx = p.first->second;
    if( p.second )
    {
        ctx = new ProbeContext();
        ctx->sock = new UDPSocket();
        if( !ctx->sock->bindToInterface(iface) || !ctx->sock->setNonBlockingMode( true ) )
        {
            delete ctx->sock;
            delete ctx;
            m_ifaceToSock.erase( p.first );
            return false;
        }

        ctx->soapWsddProxy.soap->send_timeout = SOAP_DISCOVERY_TIMEOUT;
        ctx->soapWsddProxy.soap->recv_timeout = SOAP_DISCOVERY_TIMEOUT;
        //ctx->soapWsddProxy.soap->connect_timeout = SOAP_DISCOVERY_TIMEOUT;
        //ctx->soapWsddProxy.soap->accept_timeout = SOAP_DISCOVERY_TIMEOUT;
        ctx->soapWsddProxy.soap->user = ctx->sock;
        ctx->soapWsddProxy.soap->fconnect = nullGsoapFconnect;
        ctx->soapWsddProxy.soap->fdisconnect = nullGsoapFdisconnect;
        ctx->soapWsddProxy.soap->fsend = gsoapFsendSmall;
        ctx->soapWsddProxy.soap->frecv = gsoapFrecv;
        ctx->soapWsddProxy.soap->fopen = NULL;
        ctx->soapWsddProxy.soap->socket = ctx->sock->handle();
        ctx->soapWsddProxy.soap->master = ctx->sock->handle();
    }

    //UDPSocket socket;
    //if (!socket.bindToInterface(iface))
    //    return;
    //socket.setReadTimeOut(SOAP_DISCOVERY_TIMEOUT * 1000);

    fillWsddStructs( ctx->wsddProbe, ctx->replyTo );

    char* messageID = const_cast<char*>(soap_wsa_rand_uuid(ctx->soapWsddProxy.soap));
    //qDebug() << "OnvifResourceSearcherWsdd::findEndpoints: MessageID: " << messageID << ". Interface: " << iface.address.toString();

    //String should not be changed (possibly, declaration of char* instead of const char*,- gsoap bug
    //So const_cast should be safety
    ctx->soapWsddProxy.soap_header(NULL, messageID, NULL, NULL, &ctx->replyTo, NULL,
        const_cast<char*>(WSDD_ADDRESS), const_cast<char*>(WSDD_ACTION), NULL);

    QString targetAddr = QString::fromLatin1(WSDD_GSOAP_MULTICAST_ADDRESS);
    int soapRes = ctx->soapWsddProxy.send_Probe(targetAddr.toLatin1().data(), NULL, &ctx->wsddProbe);
    soapRes = ctx->soapWsddProxy.send_Probe(targetAddr.toLatin1().data(), NULL, &ctx->wsddProbe);

    if( soapRes != SOAP_OK ) 
    {
        qWarning() << "OnvifResourceSearcherWsdd::findEndpoints: (Send) SOAP failed. GSoap error code: "
            << soapRes << SoapErrorHelper::fetchDescription(ctx->soapWsddProxy.soap_fault())
            << ". Interface: " << (iface.address.toString());
    }

    soap_destroy(ctx->soapWsddProxy.soap);
    soap_end(ctx->soapWsddProxy.soap);

    if( soapRes == SOAP_OK )
        return true;

    //removing socket for it to be recreated on next sendProbe call
    delete ctx->sock;
    delete ctx;
    m_ifaceToSock.erase( p.first );
    return false;
}

bool OnvifResourceSearcherWsdd::readProbeMatches( const QnInterfaceAndAddr& iface, EndpointInfoHash& result )
{
    std::map<QString, ProbeContext*>::iterator it = m_ifaceToSock.find( iface.address.toString() );
    if( it == m_ifaceToSock.end() )
        return false;
    ProbeContext& ctx = *it->second;

    //Receiving all ProbeMatches. Timeout = 500 ms, as written in ONVIF spec
    for( ;; )
    {
        if (m_shouldStop)
        {
            soap_destroy(ctx.soapWsddProxy.soap);
            soap_end(ctx.soapWsddProxy.soap);
            return false;
        }

        __wsdd__ProbeMatches wsddProbeMatches;
        wsddProbeMatches.wsdd__ProbeMatches = NULL;

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
            soap_destroy(ctx.soapWsddProxy.soap);
            soap_end(ctx.soapWsddProxy.soap);
            if( soapRes != SOAP_EOF )
            {
                //removing socket for it to be recreated on next sendProbe call
                delete ctx.sock;
                delete it->second;
                m_ifaceToSock.erase( it );
            }
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

            if (cl_log.logLevel() >= cl_logDEBUG1)
            {
                printProbeMatches(wsddProbeMatches.wsdd__ProbeMatches->ProbeMatch, ctx.soapWsddProxy.soap->header);
            }
        }

        soap_destroy(ctx.soapWsddProxy.soap);
        soap_end(ctx.soapWsddProxy.soap);
    }
}
