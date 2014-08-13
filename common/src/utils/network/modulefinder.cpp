/**********************************************************
* 30 oct 2012
* a.kolesnikov
***********************************************************/
#include "modulefinder.h"

#include <memory>

#include <QtCore/QDateTime>
#include <QtCore/QScopedArrayPointer>
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
    QList<QnModuleInformation> modules;

    foreach (const ModuleContext &moduleCOntext, m_knownEnterpriseControllers)
        modules.append(moduleCOntext.moduleInformation);

    return modules;
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
    response.version = qApp->applicationVersion();
    QString moduleName = qApp->applicationName();
    if (moduleName.startsWith(qApp->organizationName()))
        moduleName = moduleName.mid(qApp->organizationName().length()).trimmed();
    
    response.type = moduleName;
    response.customization = QString::fromLatin1(QN_CUSTOMIZATION_NAME);
    response.seed = qnCommon->moduleGUID();
    response.name = qnCommon->localSystemName();
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

    if (response.seed == qnCommon->moduleGUID())
        return true; // ignore requests to himself

    if (!m_compatibilityMode && response.customization.toLower() != qnProductFeatures().customizationName.toLower()) { // TODO: #2.1 #Elric #AK check for "default" VS "Vms"
        NX_LOG(QString::fromLatin1("NetworkOptixModuleFinder. Ignoring %1 (%2:%3) with different customization %4 on local address %5").
            arg(response.type).arg(remoteAddressStr).arg(remotePort).arg(response.customization).arg(udpSocket->getLocalAddress().toString()), cl_logDEBUG2);
        return false;
    }
    
    if (!m_allowedPeers.isEmpty() && !m_allowedPeers.contains(response.seed))
        return false;

    //received valid response, checking if already know this Server
    auto it = m_knownEnterpriseControllers.find(response.seed);
    bool newModule = (it == m_knownEnterpriseControllers.end());

    if (newModule)
        it = m_knownEnterpriseControllers.insert(response.seed, ModuleContext(response));

    if (!it->signalledAddresses.contains(remoteAddressStr)) {
        //new Server found

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

void QnModuleFinder::run() {
    initSystemThreadId();
    NX_LOG(lit("QnModuleFinder started"), cl_logDEBUG1);

    static const unsigned int SEARCH_PACKET_LENGTH = 64;
    quint8 searchPacket[SEARCH_PACKET_LENGTH];
    RevealRequest searchRequest;
    //serializing search packet
    quint8 *searchPacketBufStart = searchPacket;
    if (!searchRequest.serialize(&searchPacketBufStart, searchPacket + sizeof(searchPacket)))
        Q_ASSERT(false);

    foreach (AbstractDatagramSocket *socket, m_clientSockets) {
        if (!m_pollSet.add(socket, aio::etRead))
            Q_ASSERT(false);
    }
    if (m_serverSocket) {
        if (!m_pollSet.add(m_serverSocket, aio::etRead))
            Q_ASSERT(false);
    }

    while(!needToStop()) {
        quint64 currentClock = QDateTime::currentMSecsSinceEpoch();
        if (currentClock - m_prevPingClock >= m_pingTimeoutMillis) {
            //sending request via each socket
            foreach (AbstractDatagramSocket *socket, m_clientSockets) {
                if (!socket->send(searchPacket, searchPacketBufStart - searchPacket)) {
                    //failed to send packet ???
                    const SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
                    NX_LOG(lit("QnModuleFinder. Failed to send search packet to %1. %2").arg(socket->getPeerAddress().toString()).arg(SystemError::toString(prevErrorCode)), cl_logDEBUG1);
                }
            }
            m_prevPingClock = currentClock;
        }

        int socketCount = m_pollSet.poll(m_pingTimeoutMillis - (currentClock - m_prevPingClock));
        if (socketCount == 0)
            continue;    //timeout
        if (socketCount < 0) {
            const SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
            if( prevErrorCode == SystemError::interrupted )
                continue;
            NX_LOG(lit("QnModuleFinder. poll failed. %1").arg(SystemError::toString(prevErrorCode)), cl_logERROR);
            msleep(errorWaitTimeoutMs);
            continue;
        }

        currentClock = QDateTime::currentMSecsSinceEpoch();

        //some socket(s) changed state
        for (auto it = m_pollSet.begin(); it != m_pollSet.end(); ++it) {
            if (!(it.eventType() & aio::etRead))
                continue;

            AbstractDatagramSocket *udpSocket = dynamic_cast<AbstractDatagramSocket*>(it.socket());
            Q_ASSERT(udpSocket);

            if (udpSocket == m_serverSocket)
                processDiscoveryRequest(udpSocket);
            else
                processDiscoveryResponse(udpSocket);
        }

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

    NX_LOG(lit("QnModuleFinder stopped"), cl_logDEBUG1);
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
    moduleInformation.id = response.seed;
}
