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
    const unsigned defaultKeepAliveMultiply = 5;
    const unsigned errorWaitTimeoutMs = 1000;
    const unsigned checkInterfacesTimeoutMs = 60 * 1000;

} // anonymous namespace

QnMulticastModuleFinder::QnMulticastModuleFinder(
    bool clientOnly,
    const QHostAddress &multicastGroupAddress,
    const quint16 multicastGroupPort,
    const unsigned int pingTimeoutMillis,
    const unsigned int keepAliveMultiply)
:
    m_serverSocket(nullptr),
    m_pingTimeoutMillis(pingTimeoutMillis == 0 ? defaultPingTimeoutMs : pingTimeoutMillis),
    m_keepAliveMultiply(keepAliveMultiply == 0 ? defaultKeepAliveMultiply : keepAliveMultiply),
    m_prevPingClock(0),
    m_lastInterfacesCheckMs(0),
    m_compatibilityMode(false),
    m_multicastGroupAddress(multicastGroupAddress),
    m_multicastGroupPort(multicastGroupPort)
{
    if (!clientOnly) {
        m_serverSocket = new UDPSocket();
        m_serverSocket->setReuseAddrFlag(true);
        m_serverSocket->bind(SocketAddress(HostAddress::anyHost, multicastGroupPort));
    }
}

QnMulticastModuleFinder::~QnMulticastModuleFinder() {
    stop();

    qDeleteAll(m_clientSockets);
    m_clientSockets.clear();
    delete m_serverSocket;
}

bool QnMulticastModuleFinder::isValid() const {
    SCOPED_MUTEX_LOCK( lk, &m_mutex);
    return !m_clientSockets.empty();
}

bool QnMulticastModuleFinder::isCompatibilityMode() const {
    return m_compatibilityMode;
}

void QnMulticastModuleFinder::setCompatibilityMode(bool compatibilityMode) {
    m_compatibilityMode = compatibilityMode;
}

void QnMulticastModuleFinder::updateInterfaces() {
    SCOPED_MUTEX_LOCK( lk, &m_mutex);

    QList<QHostAddress> addressesToRemove = m_clientSockets.keys();

    /* This function only adds interfaces to the list. */
    for (const QHostAddress &address: QNetworkInterface::allAddresses()) {
        addressesToRemove.removeOne(address);

        if (address.protocol() != QAbstractSocket::IPv4Protocol)
            continue;

        if (m_clientSockets.contains(address))
            continue;

        try {
            //if( addressToUse == QHostAddress(lit("127.0.0.1")) )
            //    continue;
            std::unique_ptr<UDPSocket> sock( new UDPSocket() );
            sock->bind(SocketAddress(address.toString(), 0));
            sock->getLocalAddress();    //requesting local address. During this call local port is assigned to socket
            sock->setDestAddr(m_multicastGroupAddress.toString(), m_multicastGroupPort);
            auto it = m_clientSockets.insert(address, sock.release());
            if (m_serverSocket)
                m_serverSocket->joinGroup(m_multicastGroupAddress.toString(), address.toString());

            if (!m_pollSet.add(it.value()->implementationDelegate(), aio::etRead, it.value()))
                Q_ASSERT(false);
        } catch(const std::exception &e) {
            NX_LOG(lit("Failed to create socket on local address %1. %2").arg(address.toString()).arg(QString::fromLatin1(e.what())), cl_logERROR);
        }
    }

    for (const QHostAddress &address: addressesToRemove) {
        UDPSocket *socket = m_clientSockets.take(address);
        m_pollSet.remove(socket->implementationDelegate(), aio::etRead);
    }
}

QnModuleInformation QnMulticastModuleFinder::moduleInformation(const QnUuid &moduleId) const {
    SCOPED_MUTEX_LOCK( lk, &m_mutex);
    return m_foundModules[moduleId];
}

QList<QnModuleInformation> QnMulticastModuleFinder::foundModules() const {
    SCOPED_MUTEX_LOCK( lk, &m_mutex);
    return m_foundModules.values();
}

void QnMulticastModuleFinder::addIgnoredModule(const QnNetworkAddress &address, const QnUuid &id) {
    SCOPED_MUTEX_LOCK( lk, &m_mutex);
    if (m_ignoredModules.contains(address, id))
        return;

    m_ignoredModules.insert(address, id);
}

void QnMulticastModuleFinder::removeIgnoredModule(const QnNetworkAddress &address, const QnUuid &id) {
    SCOPED_MUTEX_LOCK( lk, &m_mutex);
    m_ignoredModules.remove(address, id);
}

QMultiHash<QnNetworkAddress, QnUuid> QnMulticastModuleFinder::ignoredModules() const {
    SCOPED_MUTEX_LOCK( lk, &m_mutex);
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
        NX_LOG(QString::fromLatin1("QnMulticastModuleFinder. Failed to read socket on local address (%1). %2").
            arg(udpSocket->getLocalAddress().toString()).arg(SystemError::toString(prevErrorCode)), cl_logERROR);
        return false;
    }

    //parsing received request
    RevealRequest request;
    const quint8 *requestBufStart = readBuffer;
    if (!request.deserialize(&requestBufStart, readBuffer + bytesRead)) {
        //invalid response
        NX_LOG(QString::fromLatin1("QnMulticastModuleFinder. Received invalid response from (%1:%2) on local address %3").
            arg(remoteAddressStr).arg(remotePort).arg(udpSocket->getLocalAddress().toString()), cl_logDEBUG1);
        return false;
    }

    //TODO #ak RevealResponse class is excess here. Should send/receive QnModuleInformation
    RevealResponse response(qnCommon->moduleInformation());
    quint8 *responseBufStart = readBuffer;
    if (!response.serialize(&responseBufStart, readBuffer + READ_BUFFER_SIZE))
        return false;
    if (!udpSocket->sendTo(readBuffer, responseBufStart - readBuffer, SocketAddress(remoteAddressStr, remotePort))) {
        NX_LOG(QString::fromLatin1("QnMulticastModuleFinder. Can't send response to address (%1:%2)").
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
        NX_LOG(QString::fromLatin1("QnMulticastModuleFinder. Failed to read socket on local address (%1). %2").
            arg(udpSocket->getLocalAddress().toString()).arg(SystemError::toString(prevErrorCode)), cl_logERROR);
        return false;
    }

    //parsing recevied response
    RevealResponse response;
    const quint8 *responseBufStart = readBuffer;
    if (!response.deserialize(&responseBufStart, readBuffer + bytesRead)) {
        //invalid response
        NX_LOG(QString::fromLatin1("QnMulticastModuleFinder. Received invalid response from (%1:%2) on local address %3").
            arg(remoteAddressStr).arg(remotePort).arg(udpSocket->getLocalAddress().toString()), cl_logDEBUG1);
        return false;
    }

    if (response.seed == qnCommon->moduleGUID().toString())
        return true; // ignore requests to himself

    if (response.type != nxMediaServerId && response.type != nxECId)
        return true;

    if (!m_compatibilityMode && response.customization.toLower() != qnProductFeatures().customizationName.toLower()) { // TODO: #2.1 #Elric #AK check for "default" VS "Vms"
        NX_LOG(QString::fromLatin1("QnMulticastModuleFinder. Ignoring %1 (%2:%3) with different customization %4 on local address %5").
            arg(response.type).arg(remoteAddressStr).arg(remotePort).arg(response.customization).arg(udpSocket->getLocalAddress().toString()), cl_logDEBUG2);
        return false;
    }

    QnModuleInformation moduleInformation = response.toModuleInformation();
    QnNetworkAddress address(QHostAddress(remoteAddressStr), moduleInformation.port);

    SCOPED_MUTEX_LOCK( lk, &m_mutex);

    if (m_ignoredModules.contains(address, moduleInformation.id))
        return false;

    QnModuleInformation &oldModuleInformation = m_foundModules[moduleInformation.id];

    moduleInformation.remoteAddresses.clear();
    moduleInformation.remoteAddresses.insert(remoteAddressStr);
    if (oldModuleInformation.port == moduleInformation.port) {
        moduleInformation.remoteAddresses.unite(oldModuleInformation.remoteAddresses);
    } else {
        for (const QString &oldAddress: oldModuleInformation.remoteAddresses) {
            QnNetworkAddress oldNetworkAddress(QHostAddress(oldAddress), oldModuleInformation.port);
            m_foundAddresses.remove(oldNetworkAddress);

            NX_LOG(QString::fromLatin1("QnMulticastModuleFinder. Module address (%2:%3) of remote server %1 is lost").
                arg(oldModuleInformation.id.toString()).arg(oldAddress).arg(oldModuleInformation.port), cl_logDEBUG1);

            lk.unlock();
            emit moduleAddressLost(moduleInformation, oldNetworkAddress);
            lk.relock();
        }
    }

    auto addressIt = m_foundAddresses.find(address);

    if (addressIt != m_foundAddresses.end() && addressIt->moduleId != moduleInformation.id) {
        QnModuleInformation &prevModuleInformation = m_foundModules[addressIt->moduleId];
        prevModuleInformation.remoteAddresses.remove(remoteAddressStr);

        QnModuleInformation prevModuleInformationCopy = prevModuleInformation;

        lk.unlock();

        NX_LOG(QString::fromLatin1("QnMulticastModuleFinder. Module address (%2:%3) of remote server %1 is lost").
               arg(prevModuleInformationCopy.id.toString()).arg(remoteAddressStr).arg(prevModuleInformationCopy.port), cl_logDEBUG1);
        emit moduleAddressLost(prevModuleInformationCopy, address);

        NX_LOG(QString::fromLatin1("QnMulticastModuleFinder. Module %1 has changed.").arg(prevModuleInformationCopy.id.toString()), cl_logDEBUG1);
        emit moduleChanged(prevModuleInformationCopy);

        lk.relock();
    }

    if (oldModuleInformation != moduleInformation) {
        oldModuleInformation = moduleInformation;

        lk.unlock();

        NX_LOG(QString::fromLatin1("QnMulticastModuleFinder. Module %1 has changed.").arg(response.seed.toString()), cl_logDEBUG1);
        emit moduleChanged(moduleInformation);

        lk.relock();
    }

    if (addressIt == m_foundAddresses.end()) {
        addressIt = m_foundAddresses.insert(address, ModuleContext(response));

        auto localAddress = udpSocket->getLocalAddress();

        lk.unlock();

        NX_LOG(QString::fromLatin1("QnMulticastModuleFinder. New address (%2:%3) of remote server %1 found on local interface %4").
            arg(response.seed.toString()).arg(remoteAddressStr).arg(remotePort).arg(localAddress.toString()), cl_logDEBUG1);
        emit moduleAddressFound(moduleInformation, address);

        lk.relock();
    }
    addressIt->moduleId = moduleInformation.id;
    addressIt->prevResponseReceiveClock = QDateTime::currentMSecsSinceEpoch();

    return true;
}

void QnMulticastModuleFinder::run() {
    initSystemThreadId();
    NX_LOG(lit("QnMulticastModuleFinder started"), cl_logDEBUG1);

    static const unsigned int SEARCH_PACKET_LENGTH = 64;
    quint8 searchPacket[SEARCH_PACKET_LENGTH];
    RevealRequest searchRequest;
    //serializing search packet
    quint8 *searchPacketBufStart = searchPacket;
    if (!searchRequest.serialize(&searchPacketBufStart, searchPacket + sizeof(searchPacket)))
        Q_ASSERT(false);

    //TODO #ak currently PollSet is designed for internal usage in aio, that's why we have to use socket->implementationDelegate()
    if (m_serverSocket) {
        if( !m_pollSet.add( m_serverSocket->implementationDelegate(), aio::etRead, m_serverSocket ) )
            Q_ASSERT(false);
    }

    while(!needToStop()) {
        quint64 currentClock = QDateTime::currentMSecsSinceEpoch();

        if (currentClock - m_lastInterfacesCheckMs >= checkInterfacesTimeoutMs) {
            updateInterfaces();
            m_lastInterfacesCheckMs = currentClock;
        }

        currentClock = QDateTime::currentMSecsSinceEpoch();

        if (currentClock - m_prevPingClock >= m_pingTimeoutMillis) {
            SCOPED_MUTEX_LOCK( lk, &m_mutex);

            //sending request via each socket
            for (UDPSocket *socket: m_clientSockets) {
                if (!socket->send(searchPacket, searchPacketBufStart - searchPacket)) {
                    //failed to send packet ???
                    SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
                    NX_LOG(lit("QnMulticastModuleFinder. Failed to send packet to %1. %2").arg(socket->getPeerAddress().toString()).arg(SystemError::toString(prevErrorCode)), cl_logDEBUG1);
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
            if( prevErrorCode == SystemError::interrupted )
                continue;

            NX_LOG(lit("QnMulticastModuleFinder. poll failed. %1").arg(SystemError::toString(prevErrorCode)), cl_logERROR);
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

        SCOPED_MUTEX_LOCK( lk, &m_mutex);
        //checking for expired known hosts...
        for (auto it = m_foundAddresses.begin(); it != m_foundAddresses.end(); /* no inc */) {
            QnUuid id = it->moduleId;
            QnNetworkAddress address = it.key();

            if (m_ignoredModules.contains(address, id)) {
                auto moduleIt = m_foundModules.find(id);
                if (moduleIt != m_foundModules.end() && moduleIt->remoteAddresses.remove(address.host().toString())) {
                    it = m_foundAddresses.erase(it);

                    QnModuleInformation moduleInformation = *moduleIt;

                    lk.unlock();

                    NX_LOG(QString::fromLatin1("QnMulticastModuleFinder. Module address (%2:%3) of remote server %1 is lost").
                           arg(id.toString()).arg(address.host().toString()).arg(address.port()), cl_logDEBUG1);
                    emit moduleAddressLost(moduleInformation, address);
                    NX_LOG(QString::fromLatin1("QnMulticastModuleFinder. Module %1 has changed.").arg(id.toString()), cl_logDEBUG1);
                    emit moduleChanged(moduleInformation);

                    lk.relock();

                    continue;
                }
            }

            if (it->prevResponseReceiveClock + m_pingTimeoutMillis * m_keepAliveMultiply > currentClock) {
                ++it;
                continue;
            }

            QnModuleInformation &moduleInformation = m_foundModules[id];
            moduleInformation.remoteAddresses.remove(address.host().toString());

            QnModuleInformation moduleInformationCopy = moduleInformation;

            lk.unlock();

            NX_LOG(QString::fromLatin1("QnMulticastModuleFinder. Module address (%2:%3) of remote server %1 is lost").
                arg(id.toString()).arg(address.host().toString()).arg(address.port()), cl_logDEBUG1);
            emit moduleAddressLost(moduleInformationCopy, address);

            NX_LOG(QString::fromLatin1("QnMulticastModuleFinder. Module %1 has changed.").arg(id.toString()), cl_logDEBUG1);
            emit moduleChanged(moduleInformationCopy);

            lk.relock();

            it = m_foundAddresses.erase(it);
        }
    }

    SCOPED_MUTEX_LOCK( lk, &m_mutex);
    for (UDPSocket *socket: m_clientSockets)
        m_pollSet.remove( socket->implementationDelegate(), aio::etRead );
    if (m_serverSocket)
        m_pollSet.remove( m_serverSocket->implementationDelegate(), aio::etRead );

    NX_LOG(lit("QnMulticastModuleFinder stopped"), cl_logDEBUG1);
}

QnMulticastModuleFinder::ModuleContext::ModuleContext(const RevealResponse &response)
    : prevResponseReceiveClock(0), moduleId(response.seed)
{
}
