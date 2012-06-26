#ifdef WIN32
#include "openssl/evp.h"
#else
#include "evp.h"
#endif

#include "onvif_resource_searcher_wsdd.h"
#include "core/resource/camera_resource.h"
#include "onvif_resource.h"
#include "onvif_helper.h"
//#include "onvif/Onvif.nsmap"
#include "onvif/wsaapi.h"
#include "onvif/soapwsddProxy.h"

//#include "onvif_ws_searcher_helper.h"
//#include "../digitalwatchdog/digital_watchdog_resource.h"
//#include "../brickcom/brickcom_resource.h"
//#include "../sony/sony_resource.h"

#include "utils/network/nettools.h"
//#include "utils/common/sleep.h"
//#include "utils/qtsoap/qtsoap.h"
//#include "utils/network/mac_address.h"
//#include "../digitalwatchdog/digital_watchdog_resource.h"
//#include "../brickcom/brickcom_resource.h"
//#include "utils/common/rand.h"
//#include "../sony/sony_resource.h"

#ifdef Q_OS_LINUX
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <unistd.h>
#endif


extern bool multicastJoinGroup(QUdpSocket& udpSocket, QHostAddress groupAddress, QHostAddress localAddress);
extern bool multicastLeaveGroup(QUdpSocket& udpSocket, QHostAddress groupAddress);

QString& OnvifResourceSearcherWsdd::LOCAL_ADDR = *new QString("127.0.0.1");
const char OnvifResourceSearcherWsdd::SCOPES_NAME_PREFIX[] = "onvif://www.onvif.org/name/";
const char OnvifResourceSearcherWsdd::SCOPES_HARDWARE_PREFIX[] = "onvif://www.onvif.org/hardware/";
const char OnvifResourceSearcherWsdd::PROBE_TYPE[] = "onvifDiscovery:NetworkVideoTransmitter";
const char OnvifResourceSearcherWsdd::WSA_ADDRESS[] = "http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous";
const char OnvifResourceSearcherWsdd::WSDD_ADDRESS[] = "urn:schemas-xmlsoap-org:ws:2005:04:discovery";
const char OnvifResourceSearcherWsdd::WSDD_ACTION[] = "http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe";
const char OnvifResourceSearcherWsdd::WSDD_GSOAP_MULTICAST_ADDRESS[] = "soap.udp://239.255.255.250:3702";

int WSDD_MULTICAST_PORT = 3702;
const char WSDD_MULTICAST_ADDRESS[] = "239.255.255.250";
QHostAddress WSDD_GROUP_ADDRESS(WSDD_MULTICAST_ADDRESS);


OnvifResourceSearcherWsdd::OnvifResourceSearcherWsdd():
    m_onvifFetcher(OnvifResourceInformationFetcher::instance()),
    m_recvSocketList(),
    m_mutex()
{
    updateInterfacesListenSockets();
}

OnvifResourceSearcherWsdd& OnvifResourceSearcherWsdd::instance()
{
    static OnvifResourceSearcherWsdd inst;
    return inst;
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

//Socket send through QUdpSocket
int gsoapFsend(struct soap *soap, const char *s, size_t n)
{
    QUdpSocket& qSocket = *reinterpret_cast<QUdpSocket*>(soap->user);
    qSocket.writeDatagram(QByteArray(s, n), WSDD_GROUP_ADDRESS, WSDD_MULTICAST_PORT);
    return SOAP_OK;
}

void OnvifResourceSearcherWsdd::updateInterfacesListenSockets() const
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

        QHash<QString, QUdpSocketPtr>::iterator it = m_recvSocketList.find(key);
        if (it != m_recvSocketList.end()) {
            continue;
        }

        it = m_recvSocketList.insert(key, QUdpSocketPtr(new QUdpSocket()));
        bool bindSucceeded = it.value()->bind(QHostAddress::Any, WSDD_MULTICAST_PORT, 
            QUdpSocket::ReuseAddressHint | QUdpSocket::ShareAddress);

        if (!bindSucceeded) {
            qWarning() << "OnvifResourceSearcherWsdd::updateInterfacesListenSockets: faild to bind. Address: " << key;
            continue;
        }

        if (!multicastJoinGroup(*(it.value()), WSDD_GROUP_ADDRESS, iface.address)) {
            qWarning() << "OnvifResourceSearcherWsdd::updateInterfacesListenSockets: multicast join group failed. Address: " << key;
            continue;
        }
    }
}

void OnvifResourceSearcherWsdd::findHelloEndpoints(EndpointInfoHash& result) const
{
    QMutexLocker lock(&m_mutex);

    wsddProxy soapWsddProxy(SOAP_IO_UDP);
    soapWsddProxy.soap->recv_timeout = 1;
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
        while (true) 
        {
            __wsdd__Hello wsddHello;
            wsddHello.wsdd__Hello = NULL;

            int soapRes = soapWsddProxy.recv_Hello(wsddHello);
            if (soapRes != SOAP_OK) 
            {
                if (soapRes == SOAP_EOF) 
                {
                    qDebug() << "OnvifResourceSearcherWsdd::findHelloEndpoints: All devices found. Interface: " << it.key();
                    soap_end(soapWsddProxy.soap);
                    break;
                } 
                else 
                {
                    //SOAP_NO_METHOD - The dispatcher did not find a matching operation for the request
                    //So, this is not error, silently ignore
                    if (soapRes != SOAP_NO_METHOD) {
                        qWarning() << "OnvifResourceSearcherWsdd::findHelloEndpoints: SOAP failed. GSoap error code: "
                            << soapRes << SoapErrorHelper::fetchDescription(soapWsddProxy.soap_fault())
                            << ". Interface: " << it.key();
                    }

                    soap_end(soapWsddProxy.soap);
                    continue;
                }
            }

            addEndpointToHash(result, wsddHello.wsdd__Hello, soapWsddProxy.soap->header, addrPrefixes, it.key());

            if (cl_log.logLevel() >= cl_logDEBUG1) {
                printProbeMatches(wsddHello.wsdd__Hello, soapWsddProxy.soap->header);
            }

            soap_end(soapWsddProxy.soap);
        }

        soapWsddProxy.soap->socket = SOAP_INVALID_SOCKET;
        soapWsddProxy.soap->master = SOAP_INVALID_SOCKET;
    }
}

void OnvifResourceSearcherWsdd::findEndpoints(EndpointInfoHash& result) const
{
    foreach(QnInterfaceAndAddr iface, getAllIPv4Interfaces())
    {
        QString host(iface.address.toString());

        qDebug() << "OnvifResourceSearcherWsdd::findEndpoints(): Binding to Interface: " << iface.address.toString();

        QUdpSocket qSocket;

#ifdef Q_OS_LINUX
        int res = setsockopt(qSocket.socketDescriptor(), SOL_SOCKET, SO_BINDTODEVICE, iface.name.constData(), iface.name.length());
        if (res != 0) {
            qWarning() << "OnvifResourceSearcherWsdd::findResources(): Can't bind to interface " << iface.name << ": " << strerror(errno);
            continue;
        }
#else
        if (!qSocket.bind(iface.address, 0)) 
        {
            qWarning() << "OnvifResourceSearcherWsdd::findEndpoints: QUdpSocket.bind failed. Interface: " << iface.address.toString();
            continue;
        }
#endif

        QStringList addrPrefixes = getAddrPrefixes(iface.address.toString());
        wsddProxy soapWsddProxy(SOAP_IO_UDP);
        soapWsddProxy.soap->recv_timeout = 1;
        soapWsddProxy.soap->user = &qSocket;
        soapWsddProxy.soap->fconnect = nullGsoapFconnect;
        soapWsddProxy.soap->fdisconnect = nullGsoapFdisconnect;
        soapWsddProxy.soap->fsend = gsoapFsend;
        soapWsddProxy.soap->fopen = NULL;
        soapWsddProxy.soap->socket = qSocket.socketDescriptor();
        soapWsddProxy.soap->master = qSocket.socketDescriptor();

        wsdd__ProbeType wsddProbe;
        wsa__EndpointReferenceType replyTo;
        fillWsddStructs(wsddProbe, replyTo);

        char* messageID = const_cast<char*>(soap_wsa_rand_uuid(soapWsddProxy.soap));
        qDebug() << "OnvifResourceSearcherWsdd::findEndpoints: MessageID: " << messageID << ". Interface: " << iface.address.toString();

        //String should not be changed (possibly, declaration of char* instead of const char*,- gsoap bug
        //So const_cast should be safety
        soapWsddProxy.soap_header(NULL, messageID, NULL, NULL, &replyTo, NULL,
            const_cast<char*>(WSDD_ADDRESS), const_cast<char*>(WSDD_ACTION), NULL);

        int soapRes = soapWsddProxy.send_Probe(WSDD_GSOAP_MULTICAST_ADDRESS, NULL, &wsddProbe);

        if (soapRes != SOAP_OK) 
        {
            qWarning() << "OnvifResourceSearcherWsdd::findEndpoints: (Send) SOAP failed. GSoap error code: "
                       << soapRes << SoapErrorHelper::fetchDescription(soapWsddProxy.soap_fault())
                       << ". Interface: " << iface.address.toString();
            soap_end(soapWsddProxy.soap);
            continue;
        }

        soap_end(soapWsddProxy.soap);

        //Receiving all ProbeMatches. Timeout = 500 ms, as written in ONVIF spec
        while (true) 
        {
            __wsdd__ProbeMatches wsddProbeMatches;
            wsddProbeMatches.wsdd__ProbeMatches = NULL;

            soapRes = soapWsddProxy.recv_ProbeMatches(wsddProbeMatches);
            if (soapRes != SOAP_OK) 
            {
                if (soapRes == SOAP_EOF) 
                {
                    qDebug() << "OnvifResourceSearcherWsdd::findEndpoints: All devices found. Interface: " << iface.address.toString();
                    soap_end(soapWsddProxy.soap);
                    break;
                } 
                else 
                {
                    qWarning() << "OnvifResourceSearcherWsdd::findEndpoints: SOAP failed. GSoap error code: "
                               << soapRes << SoapErrorHelper::fetchDescription(soapWsddProxy.soap_fault())
                               << ". Interface: " << iface.address.toString();
                    soap_end(soapWsddProxy.soap);
                    continue;
                }
            }

            if (wsddProbeMatches.wsdd__ProbeMatches)
            {
                addEndpointToHash(result, wsddProbeMatches.wsdd__ProbeMatches->ProbeMatch, soapWsddProxy.soap->header, addrPrefixes, iface.address.toString());

                if (cl_log.logLevel() >= cl_logDEBUG1)
                {
                    printProbeMatches(wsddProbeMatches.wsdd__ProbeMatches->ProbeMatch, soapWsddProxy.soap->header);
                }
            }

            soap_end(soapWsddProxy.soap);
        }

        soapWsddProxy.soap->socket = SOAP_INVALID_SOCKET;
        soapWsddProxy.soap->master = SOAP_INVALID_SOCKET;
        qSocket.close();
    }
}

void OnvifResourceSearcherWsdd::findResources(QnResourceList& result) const
{
    EndpointInfoHash endpoints;

    updateInterfacesListenSockets();
    findHelloEndpoints(endpoints);

    findEndpoints(endpoints);

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

    m_onvifFetcher.findResources(endpoints, result);
}

QStringList OnvifResourceSearcherWsdd::getAddrPrefixes(const QString& host) const
{
    QStringList result;

    QStringList segments = host.split(".");
    if (segments.size() != 4) {
        qCritical() << "OnvifResourceSearcherWsdd::getAddrPrefixes: is not IPv4 address: " << host;
        return result;
    }

    QString currPrefix = "http://" + segments[0] + ".";
    result.push_front(currPrefix);

    currPrefix += segments[1] + ".";
    result.push_front(currPrefix);

    currPrefix += segments[2] + ".";
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
    QString addrListStr(source->XAddrs);
    QStringList addrList = addrListStr.split(" ");
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

    qDebug() << "OnvifResourceSearcherWsdd::getAppropriateAddress: address = " << appropriateAddr
             << ". Interface: " << prefixes[0];

    return appropriateAddr;
}

template <class T>
QString OnvifResourceSearcherWsdd::getMac(const T* source, const SOAP_ENV__Header* header) const
{
    if (!source || !header) {
        return QString();
    }

    QString endpoint = source->wsa__EndpointReference.Address;
    QString messageId = header->wsa__MessageID;

    int pos = endpoint.lastIndexOf("-");
    if (pos == -1) {
        return QString();
    }
    QString macFromEndpoint = endpoint.right(endpoint.size() - pos - 1).trimmed();

    pos = messageId.lastIndexOf("-");
    if (pos == -1) {
        return QString();
    }
    QString macFromMessageId = messageId.right(messageId.size() - pos - 1).trimmed();

    if (macFromEndpoint.size() == 12 && macFromEndpoint == macFromMessageId) {
        QString result;
        for (int i = 1; i < 12; i += 2) {
            int ind = i + i / 2;
            if (i < 11) result[ind + 1] = '-';
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

    return QString(source->wsa__EndpointReference.Address);
}

template <class T>
QString OnvifResourceSearcherWsdd::getManufacturer(const T* source, const QString& name) const
{
    if (!source || !source->Scopes || !source->Scopes->__item) {
        return QString();
    }

    QString scopes(source->Scopes->__item);
    int posStart = scopes.indexOf(SCOPES_NAME_PREFIX);
    if (posStart == -1) {
        return QString();
    }

    int posEnd = posStart != -1? scopes.indexOf(" ", posStart): -1;
    posEnd = posEnd != -1? posEnd: scopes.size();

    int skipSize = sizeof(SCOPES_NAME_PREFIX) - 1;
    QString percentEncodedValue = scopes.mid(posStart + skipSize, posEnd - posStart - skipSize).replace(name, "");

    return QUrl::fromPercentEncoding(QByteArray(percentEncodedValue.toStdString().c_str())).trimmed();
}

template <class T>
QString OnvifResourceSearcherWsdd::getName(const T* source) const
{
    if (!source || !source->Scopes || !source->Scopes->__item) {
            return QString();
    }

    QString scopes(source->Scopes->__item);
    int posStart = scopes.indexOf(SCOPES_HARDWARE_PREFIX);
    if (posStart == -1) {
        return QString();
    }

    int posEnd = posStart != -1? scopes.indexOf(" ", posStart): -1;
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
    QString uniqId = !mac.isEmpty()? mac: getEndpointAddress(source);

    hash.insert(appropriateAddr, EndpointAdditionalInfo(name, manufacturer, mac, uniqId, host));
}

template <class T>
void OnvifResourceSearcherWsdd::printProbeMatches(const T* source, const SOAP_ENV__Header* header) const
{
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
