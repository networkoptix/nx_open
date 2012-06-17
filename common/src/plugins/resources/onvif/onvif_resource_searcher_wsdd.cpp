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
const char OnvifResourceSearcherWsdd::WSDD_MULTICAST_ADDRESS[] = "soap.udp://239.255.255.250:3702";


OnvifResourceSearcherWsdd::OnvifResourceSearcherWsdd():
    onvifFetcher(OnvifResourceInformationFetcher::instance())
{

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
    qSocket.writeDatagram(QByteArray(s, n), QHostAddress("239.255.255.250"), 3702);
    return SOAP_OK;
}

void OnvifResourceSearcherWsdd::findEndpoints(EndpointInfoHash& result) const
{
#ifdef Q_OS_LINUX
    struct ifaddrs *ifaddr, *ifa;
    char hostBuf[NI_MAXHOST + 1];

    if (getifaddrs(&ifaddr) == -1) {
        qWarning() << "OnvifResourceSearcherWsdd::findEndpoints(): Can't get interfaces list: " << strerror(errno);
        return;
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr->sa_family != AF_INET)
            continue;

        int s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in), hostBuf, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
        hostBuf[NI_MAXHOST] = '\0';
        if (s != 0) {
            qWarning() << "OnvifResourceSearcherWsdd::findEndpoints(): getnameinfo() failed: " << strerror(errno);
            continue;
        }

        QString host(hostBuf);

        if (host == LOCAL_ADDR) {
            continue;
        }

        QHostAddress localAddress(host);

#elif defined Q_OS_WIN
    QList<QHostAddress> localAddresses = getAllIPv4Addresses();

    foreach(QHostAddress localAddress, localAddresses)
    {
        QString host(localAddress.toString());
#endif
        qDebug() << "OnvifResourceSearcherWsdd::findEndpoints(): Binding to Interface: " << host;

        QUdpSocket qSocket;
        if (!qSocket.bind(localAddress, 0)) {
            qWarning() << "OnvifResourceSearcherWsdd::findEndpoints: QUdpSocket.bind failed. Interface: " << host;
            continue;
        }

        QStringList addrPrefixes = getAddrPrefixes(host);
        wsddProxy soapWsddProxy(SOAP_IO_UDP);
        soapWsddProxy.soap->recv_timeout = -500000;
        soapWsddProxy.soap->user = &qSocket;
        soapWsddProxy.soap->fconnect = nullGsoapFconnect;
        soapWsddProxy.soap->fdisconnect = nullGsoapFdisconnect;
        soapWsddProxy.soap->fsend = gsoapFsend;
        soapWsddProxy.soap->fopen = NULL;
        soapWsddProxy.soap->socket = qSocket.socketDescriptor();
        soapWsddProxy.soap->master = qSocket.socketDescriptor();

#ifdef Q_OS_LINUX
        int res = setsockopt(qSocket.socketDescriptor(), SOL_SOCKET, SO_BINDTODEVICE, ifa->ifa_name, strlen(ifa->ifa_name));
        if (res != 0) {
            qWarning() << "QnPlDlinkResourceSearcher::findResources(): Can't bind to interface " << ifa->ifa_name << ": " << strerror(errno);
            continue;
        }
#endif

        wsdd__ProbeType wsddProbe;
        wsa__EndpointReferenceType replyTo;
        fillWsddStructs(wsddProbe, replyTo);

        char* messageID = const_cast<char*>(soap_wsa_rand_uuid(soapWsddProxy.soap));
        qDebug() << "OnvifResourceSearcherWsdd::findEndpoints: MessageID: " << messageID << ". Interface: " << host;;

        //String should not be changed (possibly, declaration of char* instead of const char*,- gsoap bug
        //So const_cast should be safety
        soapWsddProxy.soap_header(NULL, messageID, NULL, NULL, &replyTo, NULL,
            const_cast<char*>(WSDD_ADDRESS), const_cast<char*>(WSDD_ACTION), NULL);

        int soapRes = soapWsddProxy.send_Probe(WSDD_MULTICAST_ADDRESS, NULL, &wsddProbe);
        if (soapRes != SOAP_OK) {
            qWarning() << "OnvifResourceSearcherWsdd::findEndpoints: (Send) SOAP failed. GSoap error code: "
                       << soapRes << SoapErrorHelper::fetchDescription(soapWsddProxy.soap_fault())
                       << ". Interface: " << host;
            soap_end(soapWsddProxy.soap);
            continue;
        }
        soap_end(soapWsddProxy.soap);

        //Receiving all ProbeMatches. Timeout = 500 ms, as written in ONVIF spec
        while (true) {
            __wsdd__ProbeMatches wsddProbeMatches;
            wsddProbeMatches.wsdd__ProbeMatches = NULL;

            soapRes = soapWsddProxy.recv_ProbeMatches(wsddProbeMatches);
            if (soapRes != SOAP_OK) {
                if (soapRes == SOAP_EOF) {
                    qDebug() << "OnvifResourceSearcherWsdd::findEndpoints: All devices found. Interface: " << host;
                } else {
                    qWarning() << "OnvifResourceSearcherWsdd::findEndpoints: SOAP failed. GSoap error code: "
                               << soapRes << SoapErrorHelper::fetchDescription(soapWsddProxy.soap_fault())
                               << ". Interface: " << host;
                }

                soap_end(soapWsddProxy.soap);
                break;
            }

            addEndpointToHash(result, wsddProbeMatches.wsdd__ProbeMatches, soapWsddProxy.soap->header, addrPrefixes, host);

            if (cl_log.logLevel() >= cl_logDEBUG1) {
                printProbeMatches(wsddProbeMatches.wsdd__ProbeMatches, soapWsddProxy.soap->header);
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
    findEndpoints(endpoints);

    //ToDo: delete
    EndpointInfoHash::ConstIterator endpIter = endpoints.begin();
    qDebug() << "OnvifResourceSearcherWsdd::findResources: Endpoints in the list:"
             << (endpoints.size()? "": " EMPTY");
    while (endpIter != endpoints.end()) {
        qDebug() << "    " << endpIter.key() << " (" << endpIter.value().mac << "): " << endpIter.value().manufacturer
                 << " - " << endpIter.value().name << ", discovered in " << endpIter.value().discoveryIp;
        ++endpIter;
    }

    onvifFetcher.findResources(endpoints, result);
}

const QStringList OnvifResourceSearcherWsdd::getAddrPrefixes(const QString& host) const
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

const QString OnvifResourceSearcherWsdd::getAppropriateAddress(
        const wsdd__ProbeMatchesType* probeMatches, const QStringList& prefixes) const
{
    QString appropriateAddr;

    if (!probeMatches || !probeMatches->ProbeMatch || !probeMatches->ProbeMatch->XAddrs) {
        return appropriateAddr;
    }

    int relevantLevel = 0;
    QString addrListStr(probeMatches->ProbeMatch->XAddrs);
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

const QString OnvifResourceSearcherWsdd::getMac(const wsdd__ProbeMatchesType* probeMatches, const SOAP_ENV__Header* header) const
{
    if (!probeMatches || !probeMatches->ProbeMatch || !header) {
        return QString();
    }

    QString endpoint = probeMatches->ProbeMatch->wsa__EndpointReference.Address;
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

        return result;
    }

    return QString();
}

const QString OnvifResourceSearcherWsdd::getManufacturer(const wsdd__ProbeMatchesType* probeMatches, const QString& name) const
{
    if (!probeMatches || !probeMatches->ProbeMatch ||
            !probeMatches->ProbeMatch->Scopes || !probeMatches->ProbeMatch->Scopes->__item) {
        return QString();
    }

    QString scopes(probeMatches->ProbeMatch->Scopes->__item);
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

const QString OnvifResourceSearcherWsdd::getName(const wsdd__ProbeMatchesType* probeMatches) const
{
    if (!probeMatches || !probeMatches->ProbeMatch ||
        !probeMatches->ProbeMatch->Scopes || !probeMatches->ProbeMatch->Scopes->__item) {
            return QString();
    }

    QString scopes(probeMatches->ProbeMatch->Scopes->__item);
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

void OnvifResourceSearcherWsdd::addEndpointToHash(EndpointInfoHash& hash, const wsdd__ProbeMatchesType* probeMatches,
    const SOAP_ENV__Header* header, const QStringList& addrPrefixes, const QString& host) const
{
    if (!probeMatches || !probeMatches->ProbeMatch) {
        return;
    }

    QString appropriateAddr = getAppropriateAddress(probeMatches, addrPrefixes);
    if (appropriateAddr.isEmpty() || hash.contains(appropriateAddr)) {
        return;
    }

    QString name = getName(probeMatches);
    QString manufacturer = getManufacturer(probeMatches, name);
    QString mac = getMac(probeMatches, header);

    hash.insert(appropriateAddr, EndpointAdditionalInfo(name, manufacturer, mac, host));
}

void OnvifResourceSearcherWsdd::printProbeMatches(const wsdd__ProbeMatchesType* probeMatches,
    const SOAP_ENV__Header* header) const
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
    if (probeMatches && probeMatches->ProbeMatch) {

        if (probeMatches->ProbeMatch->MetadataVersion) {
            qDebug() << "    MetadataVersion: " << probeMatches->ProbeMatch->MetadataVersion;
        }

        if (probeMatches->ProbeMatch->Types) {
            qDebug() << "    Types: " << probeMatches->ProbeMatch->Types;
        }

        if (probeMatches->ProbeMatch->wsa__EndpointReference.Address) {
            qDebug() << "    Address: " << probeMatches->ProbeMatch->wsa__EndpointReference.Address;
        }

        if (probeMatches->ProbeMatch->Scopes && probeMatches->ProbeMatch->Scopes->__item) {
            qDebug() << "    Scopes: " << probeMatches->ProbeMatch->Scopes->__item;
        }

        if (probeMatches->ProbeMatch->XAddrs) {
            qDebug() << "    XAddrs: " << probeMatches->ProbeMatch->XAddrs;
        }

    } else {
        qDebug() << "    EMPTY";
    }
}
