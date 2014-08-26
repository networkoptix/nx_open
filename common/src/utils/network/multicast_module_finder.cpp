#include "multicast_module_finder.h"

#include <memory>

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtNetwork/QNetworkInterface>

#include <utils/common/log.h>
#include <utils/common/systemerror.h>
#include <utils/common/product_features.h>

#include "socket.h"
#include "system_socket.h"
#include "common/common_module.h"


namespace {

    const unsigned defaultPingTimeoutMs = 3000;
    const unsigned defaultKeepAliveMultiply = 3;
    const unsigned errorWaitTimeoutMs = 1000;

    QUrl makeUrl(const QHostAddress &address, quint16 port) {
        return QUrl(lit("http://%1:%2").arg(address.toString()).arg(port));
    }

} // anonymous namespace

QnMulticastModuleFinder::QnMulticastModuleFinder(
    bool clientOnly,
    const QHostAddress &multicastGroupAddress,
    const unsigned int multicastGroupPort,
    const unsigned int pingTimeoutMillis,
    const unsigned int keepAliveMultiply)
:
    m_serverSocket(nullptr),
    m_pingTimeoutMillis(pingTimeoutMillis == 0 ? defaultPingTimeoutMs : pingTimeoutMillis),
    m_keepAliveMultiply(keepAliveMultiply == 0 ? defaultKeepAliveMultiply : keepAliveMultiply),
    m_prevPingClock(0),
    m_compatibilityMode(false)
{
    if (!clientOnly) {
        m_serverSocket = new UDPSocket();
        m_serverSocket->setReuseAddrFlag(true);
        m_serverSocket->bind(SocketAddress(HostAddress::anyHost, multicastGroupPort));
    }

    foreach (const QHostAddress &address, QNetworkInterface::allAddresses()) {
        if (address.protocol() != QAbstractSocket::IPv4Protocol)
            continue;

        try {
            //if( addressToUse == QHostAddress(lit("127.0.0.1")) )
            //    continue;
            std::unique_ptr<UDPSocket> sock( new UDPSocket() );
            sock->bind(SocketAddress(address.toString(), 0));
            sock->getLocalAddress();    //requesting local address. During this call local port is assigned to socket
            sock->setDestAddr(multicastGroupAddress.toString(), multicastGroupPort);
            m_clientSockets.push_back(sock.release());
            m_localNetworkAdresses.insert(address.toString());
            if (m_serverSocket)
                m_serverSocket->joinGroup(multicastGroupAddress.toString(), address.toString());
        } catch(const std::exception &e) {
            NX_LOG(lit("Failed to create socket on local address %1. %2").arg(address.toString()).arg(QString::fromLatin1(e.what())), cl_logERROR);
        }
    }
}

QnMulticastModuleFinder::~QnMulticastModuleFinder() {
    stop();

    qDeleteAll(m_clientSockets);
    m_clientSockets.clear();
    delete m_serverSocket;
}

bool QnMulticastModuleFinder::isValid() const {
    return !m_clientSockets.empty();
}

bool QnMulticastModuleFinder::isCompatibilityMode() const {
    return m_compatibilityMode;
}

void QnMulticastModuleFinder::setCompatibilityMode(bool compatibilityMode) {
    m_compatibilityMode = compatibilityMode;
}

QList<QnModuleInformation> QnMulticastModuleFinder::revealedModules() const {
    QMutexLocker lk(&m_mutex);

    QList<QnModuleInformation> modules;
    foreach (const ModuleContext &moduleCOntext, m_knownEnterpriseControllers)
        modules.append(moduleCOntext.moduleInformation);

    return modules;
}

QnModuleInformation QnMulticastModuleFinder::moduleInformation(const QString &moduleId) const {
    QMutexLocker lk(&m_mutex);

    auto it = m_knownEnterpriseControllers.find(moduleId);
    if (it == m_knownEnterpriseControllers.end())
        return QnModuleInformation();

    return it->moduleInformation;
}

void QnMulticastModuleFinder::addIgnoredModule(const QHostAddress &address, quint16 port, const QUuid &id) {
    QUrl url = makeUrl(address, port);
    if (!m_ignoredModules.contains(url, id))
        m_ignoredModules.insert(url, id);
}

void QnMulticastModuleFinder::removeIgnoredModule(const QHostAddress &address, quint16 port, const QUuid &id) {
    QUrl url = makeUrl(address, port);
    m_ignoredModules.remove(url, id);
}

QMultiHash<QUrl, QUuid> QnMulticastModuleFinder::ignoredModules() const {
    return m_ignoredModules;
}

void QnMulticastModuleFinder::pleaseStop() {
    QnLongRunnable::pleaseStop();
    m_pollSet.interrupt();
}

bool QnMulticastModuleFinder::processDiscoveryRequest(UDPSocket *udpSocket) {
    static const size_t READ_BUFFER_SIZE = UDPSocket::MAX_PACKET_SIZE;
    quint8 readBuffer[READ_BUFFER_SIZE];

    QString remoteAddressStr;
    unsigned short remotePort = 0;
    int bytesRead = udpSocket->recvFrom(readBuffer, READ_BUFFER_SIZE, remoteAddressStr, remotePort);
    if (bytesRead == -1) {
        SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
        NX_LOG(QString::fromLatin1("NetworkOptixModuleFinder. Failed to read socket on local address (%1). %2").
            arg(udpSocket->getLocalAddress().toString()).arg(SystemError::toString(prevErrorCode)), cl_logERROR);
        return false;
    }

    //parsing received request
    RevealRequest request;
    const quint8 *requestBufStart = readBuffer;
    if (!request.deserialize(&requestBufStart, readBuffer + bytesRead)) {
        //invalid response
        NX_LOG(QString::fromLatin1("NetworkOptixModuleFinder. Received invalid response from (%1:%2) on local address %3").
            arg(remoteAddressStr).arg(remotePort).arg(udpSocket->getLocalAddress().toString()), cl_logDEBUG1);
        return false;
    }

    //TODO #ak RevealResponse class is excess here. Should send/receive QnModuleInformation
    const QnModuleInformation& selfModuleInformation = qnCommon->moduleInformation();
    RevealResponse response;
    response.version = selfModuleInformation.version.toString();
    response.type = selfModuleInformation.type;
    response.customization = selfModuleInformation.customization;
    response.seed = selfModuleInformation.id.toString();
    response.name = selfModuleInformation.systemName;
    response.systemInformation = selfModuleInformation.systemInformation.toString();
    response.sslAllowed = selfModuleInformation.sslAllowed;
    response.typeSpecificParameters.insert(lit("port"), QString::number(selfModuleInformation.port));
    quint8 *responseBufStart = readBuffer;
    if (!response.serialize(&responseBufStart, readBuffer + READ_BUFFER_SIZE))
        return false;
    if (!udpSocket->sendTo(readBuffer, responseBufStart - readBuffer, SocketAddress(remoteAddressStr, remotePort))) {
        NX_LOG(QString::fromLatin1("NetworkOptixModuleFinder. Can't send response to address (%1:%2)").
            arg(remoteAddressStr).arg(remotePort), cl_logDEBUG1);
        return false;

    };

    return true;
}

bool QnMulticastModuleFinder::processDiscoveryResponse(UDPSocket *udpSocket) {
    static const size_t READ_BUFFER_SIZE = UDPSocket::MAX_PACKET_SIZE;
    quint8 readBuffer[READ_BUFFER_SIZE];

    //reading socket response
    QString remoteAddressStr;
    unsigned short remotePort = 0;
    int bytesRead = udpSocket->recvFrom(readBuffer, READ_BUFFER_SIZE, remoteAddressStr, remotePort);
    if (bytesRead == -1) {
        SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
        NX_LOG(QString::fromLatin1("NetworkOptixModuleFinder. Failed to read socket on local address (%1). %2").
            arg(udpSocket->getLocalAddress().toString()).arg(SystemError::toString(prevErrorCode)), cl_logERROR);
        return false;
    }

    //parsing recevied response
    RevealResponse response;
    const quint8 *responseBufStart = readBuffer;
    if (!response.deserialize(&responseBufStart, readBuffer + bytesRead)) {
        //invalid response
        NX_LOG(QString::fromLatin1("NetworkOptixModuleFinder. Received invalid response from (%1:%2) on local address %3").
            arg(remoteAddressStr).arg(remotePort).arg(udpSocket->getLocalAddress().toString()), cl_logDEBUG1);
        return false;
    }

    if (response.seed == qnCommon->moduleGUID().toString())
        return true; // ignore requests to himself

    if (!m_compatibilityMode && response.customization.toLower() != qnProductFeatures().customizationName.toLower()) { // TODO: #2.1 #Elric #AK check for "default" VS "Vms"
        NX_LOG(QString::fromLatin1("NetworkOptixModuleFinder. Ignoring %1 (%2:%3) with different customization %4 on local address %5").
            arg(response.type).arg(remoteAddressStr).arg(remotePort).arg(response.customization).arg(udpSocket->getLocalAddress().toString()), cl_logDEBUG2);
        return false;
    }

    if (m_ignoredModules.contains(makeUrl(QHostAddress(remoteAddressStr), remotePort), response.seed))
        return false;

    QMutexLocker lk(&m_mutex);

    //received valid response, checking if already know this enterprise controller
    auto it = m_knownEnterpriseControllers.find(response.seed);
    bool newModule = (it == m_knownEnterpriseControllers.end());

    if (newModule)
        it = m_knownEnterpriseControllers.insert(response.seed, ModuleContext(response));

    if (!it->signalledAddresses.contains(remoteAddressStr)) {
        //new enterprise controller found

        it->signalledAddresses.insert(remoteAddressStr);
        const QHostAddress &localAddress = QHostAddress(udpSocket->getLocalAddress().address.toString());
        if (newModule) { // new module has been found
            NX_LOG(QString::fromLatin1("NetworkOptixModuleFinder. New remote server of type %1 found at address (%2:%3) on local interface %4").
                arg(response.type).arg(remoteAddressStr).arg(remotePort).arg(localAddress.toString()), cl_logDEBUG1);
        } else { // new address of existing module
            NX_LOG(QString::fromLatin1("NetworkOptixModuleFinder. New address (%2:%3) of remote server of type %1 found on local interface %4").
                arg(response.type).arg(remoteAddressStr).arg(remotePort).arg(localAddress.toString()), cl_logDEBUG1);
        }

        it->moduleInformation.remoteAddresses.insert(remoteAddressStr);
        it->moduleInformation.isLocal |= m_localNetworkAdresses.contains(remoteAddressStr);

        emit moduleFound(it->moduleInformation, remoteAddressStr, localAddress.toString());
    }
    it->prevResponseReceiveClock = QDateTime::currentMSecsSinceEpoch();

    return true;
}

void QnMulticastModuleFinder::run() {
    initSystemThreadId();
    NX_LOG(lit("NetworkOptixModuleFinder started"), cl_logDEBUG1);

    static const unsigned int SEARCH_PACKET_LENGTH = 64;
    quint8 searchPacket[SEARCH_PACKET_LENGTH];
    RevealRequest searchRequest;
    //serializing search packet
    quint8 *searchPacketBufStart = searchPacket;
    if (!searchRequest.serialize(&searchPacketBufStart, searchPacket + sizeof(searchPacket)))
        Q_ASSERT(false);

    foreach (UDPSocket *socket, m_clientSockets) {
        if( !m_pollSet.add( socket->implementationDelegate(), aio::etRead, socket ) )
            Q_ASSERT(false);
    }
    if (m_serverSocket) {
        if( !m_pollSet.add( m_serverSocket->implementationDelegate(), aio::etRead, m_serverSocket ) )
            Q_ASSERT(false);
    }

    while(!needToStop()) {
        quint64 currentClock = QDateTime::currentMSecsSinceEpoch();
        if (currentClock - m_prevPingClock >= m_pingTimeoutMillis) {
            //sending request via each socket
            foreach (UDPSocket *socket, m_clientSockets) {
                if (!socket->send(searchPacket, searchPacketBufStart - searchPacket)) {
                    //failed to send packet ???
                    SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
                    NX_LOG(lit("NetworkOptixModuleFinder. poll failed. %1").arg(SystemError::toString(prevErrorCode)), cl_logDEBUG1);
                    //TODO/IMPL if corresponding interface is down, should remove socket from set
                }
            }
            m_prevPingClock = currentClock;
        }

        int socketCount = m_pollSet.poll(m_pingTimeoutMillis - (currentClock - m_prevPingClock));
        if (socketCount == 0)
            continue;    //timeout
        if (socketCount < 0) {
            SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
            NX_LOG(lit("NetworkOptixModuleFinder. poll failed. %1").arg(SystemError::toString(prevErrorCode)), cl_logERROR);
            msleep(errorWaitTimeoutMs);
            continue;
        }

        currentClock = QDateTime::currentMSecsSinceEpoch();

        //some socket(s) changed state
        for (auto it = m_pollSet.begin(); it != m_pollSet.end(); ++it) {
            if (!(it.eventType() & aio::etRead))
                continue;

            UDPSocket* udpSocket = static_cast<UDPSocket*>(it.userData());

            if (udpSocket == m_serverSocket)
                processDiscoveryRequest(udpSocket);
            else
                processDiscoveryResponse(udpSocket);
        }

        QMutexLocker lk(&m_mutex);
        //checking for expired known hosts...
        for (auto it = m_knownEnterpriseControllers.begin(); it != m_knownEnterpriseControllers.end(); /* no inc */) {
            if(it->prevResponseReceiveClock + m_pingTimeoutMillis*m_keepAliveMultiply > currentClock) {
                ++it;
                continue;
            }

            emit moduleLost(it->moduleInformation);
            it = m_knownEnterpriseControllers.erase(it);
        }
    }

    NX_LOG(lit("NetworkOptixModuleFinder stopped"), cl_logDEBUG1);
}

QnMulticastModuleFinder::ModuleContext::ModuleContext(const RevealResponse &response)
    : response(response),
      prevResponseReceiveClock(0)
{
    moduleInformation.type = response.type;
    moduleInformation.version = QnSoftwareVersion(response.version);
    moduleInformation.systemInformation = QnSystemInformation(response.systemInformation);
    moduleInformation.systemName = response.name;
    moduleInformation.port = response.typeSpecificParameters.value(lit("port")).toUShort();
    moduleInformation.id = QUuid(response.seed);
    moduleInformation.sslAllowed = response.sslAllowed;
}
