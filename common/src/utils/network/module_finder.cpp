/**********************************************************
* 30 oct 2012
* a.kolesnikov
***********************************************************/
#include "module_finder.h"

#include <memory>

#include <QtCore/QDateTime>
#include <QtCore/QScopedArrayPointer>
#include <QtCore/QTimer>
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
    const unsigned directCheckIntervalMs = 10000;
    const unsigned directCheckSocketTimeoutMs = 3000;

} // anonymous namespace

QnModuleFinder::QnModuleFinder(
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
            std::auto_ptr<AbstractDatagramSocket> sock(SocketFactory::createDatagramSocket());
            sock->bind(address.toString(), 0);
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

QnModuleFinder::~QnModuleFinder() {
    stop();
    
    qDeleteAll(m_clientSockets);
    m_clientSockets.clear();
    delete m_serverSocket;
}

bool QnModuleFinder::isValid() const {
    return !m_clientSockets.empty();
}

bool QnModuleFinder::isCompatibilityMode() const {
    return m_compatibilityMode;
}

void QnModuleFinder::setCompatibilityMode(bool compatibilityMode) {
    m_compatibilityMode = compatibilityMode;
}

QList<QnModuleInformation> QnModuleFinder::revealedModules() const {
    QMutexLocker lk(&m_mutex);

    QList<QnModuleInformation> modules;
    foreach (const ModuleContext &moduleCOntext, m_knownEnterpriseControllers)
        modules.append(moduleCOntext.moduleInformation);

    return modules;
}

QnModuleInformation QnModuleFinder::moduleInformation(const QString &moduleId) const {
    QMutexLocker lk(&m_mutex);

    auto it = m_knownEnterpriseControllers.find(moduleId);
    if (it == m_knownEnterpriseControllers.end())
        return QnModuleInformation();

    return it->moduleInformation;
}

void QnModuleFinder::addManualCheckAddress(const QString &address, unsigned short port) {
    FullAddress fullAddress(address, port);

    QMutexLocker lk(&m_mutex);

    if (!m_manualCheckAddresses.contains(fullAddress))
        m_manualCheckAddresses.append(fullAddress);
}

void QnModuleFinder::removeManualCheckAddress(const QString &address, unsigned short port) {
    FullAddress fullAddress(address, port);

    QMutexLocker lk(&m_mutex);

    m_manualCheckAddresses.removeOne(fullAddress);
}

void QnModuleFinder::addAutoCheckAddress(const QString &address, unsigned short port) {
    FullAddress fullAddress(address, port);

    QMutexLocker lk(&m_mutex);

    if (!m_manualCheckAddresses.contains(fullAddress) && !m_autoCheckAddresses.contains(fullAddress))
        m_manualCheckAddresses.append(fullAddress);

    m_directCheckQueue.enqueue(fullAddress);
}

void QnModuleFinder::removeAutoCheckAddress(const QString &address, unsigned short port) {
    FullAddress fullAddress(address, port);

    QMutexLocker lk(&m_mutex);

    m_autoCheckAddresses.removeOne(fullAddress);

    if (!m_manualCheckAddresses.contains(fullAddress))
        m_directCheckQueue.removeOne(fullAddress);
}

void QnModuleFinder::pleaseStop() {
    QnLongRunnable::pleaseStop();
    m_pollSet.interrupt();
}

bool QnModuleFinder::processDiscoveryRequest(AbstractDatagramSocket *udpSocket) {
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

    RevealResponse response;
    response.version = qnCommon->engineVersion().toString();
    QString moduleName = qApp->applicationName();
    if (moduleName.startsWith(qApp->organizationName()))
        moduleName = moduleName.mid(qApp->organizationName().length()).trimmed();
    
    response.type = moduleName;
    response.customization = QString::fromLatin1(QN_CUSTOMIZATION_NAME);
    response.seed = qnCommon->moduleGUID().toString();
    response.name = qnCommon->localSystemName();
    response.systemInformation = QnSystemInformation(lit(QN_APPLICATION_PLATFORM), lit(QN_APPLICATION_ARCH), lit(QN_ARM_BOX)).toString();
    response.typeSpecificParameters.insert(lit("port"), QString::number(qnCommon->moduleUrl().port()));
    quint8 *responseBufStart = readBuffer;
    if (!response.serialize(&responseBufStart, readBuffer + READ_BUFFER_SIZE))
        return false;
    if (!udpSocket->sendTo(readBuffer, responseBufStart - readBuffer, remoteAddressStr, remotePort)) {
        NX_LOG(QString::fromLatin1("NetworkOptixModuleFinder. Can't send response to address (%1:%2)").
            arg(remoteAddressStr).arg(remotePort), cl_logDEBUG1);
        return false;

    };

    return true;
}

bool QnModuleFinder::processDiscoveryResponse(AbstractDatagramSocket *udpSocket) {
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

        m_knownAddresses.insert(FullAddress(remoteAddressStr, response.typeSpecificParameters[lit("port")].toInt()));

        emit moduleFound(it->moduleInformation, remoteAddressStr, localAddress.toString());
    }
    it->prevResponseReceiveClock = QDateTime::currentMSecsSinceEpoch();

    return true;
}

void QnModuleFinder::removeDirectCheckSocket(AbstractDatagramSocket *socket) {
    if (m_directCheckSockets.contains(udpSocket)) {
        m_directCheckSocketCreationTime.remove(udpSocket);
        m_directCheckSockets.removeOne(udpSocket);
        m_pollSet.remove(udpSocket);
        delete udpSocket;
    }
}

void QnModuleFinder::run() {
    initSystemThreadId();
    NX_LOG(lit("NetworkOptixModuleFinder started"), cl_logDEBUG1);

    static const unsigned int SEARCH_PACKET_LENGTH = 64;
    quint8 searchPacket[SEARCH_PACKET_LENGTH];
    RevealRequest searchRequest;
    //serializing search packet
    quint8 *searchPacketBufStart = searchPacket;
    if (!searchRequest.serialize(&searchPacketBufStart, searchPacket + sizeof(searchPacket)))
        Q_ASSERT(false);

    foreach (AbstractDatagramSocket *socket, m_clientSockets) {
        if (!m_pollSet.add(socket, PollSet::etRead))
            Q_ASSERT(false);
    }
    if (m_serverSocket) {
        if (!m_pollSet.add(m_serverSocket, PollSet::etRead))
            Q_ASSERT(false);
    }

    while(!needToStop()) {
        quint64 currentClock = QDateTime::currentMSecsSinceEpoch();

        // remove timed out sockets
        foreach (AbstractDatagramSocket *socket, m_directCheckSockets) {
            if (currentClock - directCheckSocketTimeoutMs >= m_directCheckSocketCreationTime[socket])
                removeDirectCheckSocket(socket);
        }

        // enqueue manual addresses
        if (currentClock - m_lastDirectCheckClock >= directCheckIntervalMs) {
            foreach (const FullAddress &address, m_manualCheckAddresses) {
                if (m_knownAddresses.contains(address))
                    continue;

                if (m_directCheckQueue.contains(address))
                    continue;

                m_directCheckQueue.enqueue(address);
            }
        }

        // create sockets for the queued addresses
        while (!m_directCheckQueue.isEmpty()) {
            FullAddress address = m_directCheckQueue.dequeue();
            AbstractDatagramSocket *socket = SocketFactory::createDatagramSocket();
            socket->setDestAddr(address.address, address.port);
            m_directCheckSockets.append(DirectCheckSocket(socket, currentClock));
            m_pollSet.add(socket);
        }

        currentClock = QDateTime::currentMSecsSinceEpoch();
        if (currentClock - m_prevPingClock >= m_pingTimeoutMillis) {
            //sending request via each socket
            foreach (AbstractDatagramSocket *socket, m_clientSockets) {
                if (!socket->send(searchPacket, searchPacketBufStart - searchPacket)) {
                    //failed to send packet ???
                    SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
                    NX_LOG(lit("NetworkOptixModuleFinder. poll failed. %1").arg(SystemError::toString(prevErrorCode)), cl_logDEBUG1);
                    //TODO/IMPL if corresponding interface is down, should remove socket from set
                }
            }
            // do the same for direct check sockets
            foreach (AbstractDatagramSocket *socket, m_directCheckSockets) {
                if (!socket->send(searchPacket, searchPacketBufStart - searchPacket)) {
                    SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
                    NX_LOG(lit("NetworkOptixModuleFinder. poll failed. %1").arg(SystemError::toString(prevErrorCode)), cl_logDEBUG1);
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
            if (!(it.eventType() & PollSet::etRead))
                continue;

            AbstractDatagramSocket *udpSocket = dynamic_cast<AbstractDatagramSocket*>(it.socket());
            Q_ASSERT(udpSocket);

            if (udpSocket == m_serverSocket)
                processDiscoveryRequest(udpSocket);
            else
                processDiscoveryResponse(udpSocket);

            removeDirectCheckSocket(udpSocket);
        }

        QMutexLocker lk(&m_mutex);
        //checking for expired known hosts...
        for (auto it = m_knownEnterpriseControllers.begin(); it != m_knownEnterpriseControllers.end(); /* no inc */) {
            if(it->prevResponseReceiveClock + m_pingTimeoutMillis*m_keepAliveMultiply > currentClock) {
                ++it;
                continue;
            }

            foreach (const QString &address, it->moduleInformation.remoteAddresses)
                m_knownAddresses.remove(address, it->moduleInformation.parameters[lit("port")].toInt());

            emit moduleLost(it->moduleInformation);
            it = m_knownEnterpriseControllers.erase(it);
        }
    }

    NX_LOG(lit("NetworkOptixModuleFinder stopped"), cl_logDEBUG1);
}

QnModuleFinder::ModuleContext::ModuleContext(const RevealResponse &response)
    : response(response),
      prevResponseReceiveClock(0)
{
    moduleInformation.type = response.type;
    moduleInformation.version = QnSoftwareVersion(response.version);
    moduleInformation.systemInformation = QnSystemInformation(response.systemInformation);
    moduleInformation.systemName = response.name;
    moduleInformation.parameters = response.typeSpecificParameters;
    moduleInformation.id = QnId(response.seed);
}
