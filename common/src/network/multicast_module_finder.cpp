#include "multicast_module_finder.h"

#include <memory>

#include <QtCore/QCoreApplication>
#include <QtCore/QDateTime>
#include <QtNetwork/QNetworkInterface>

#include <nx/utils/log/log.h>
#include <utils/common/systemerror.h>
#include <nx/utils/std/cpp14.h>

#include <nx/network/socket.h>
#include <nx/network/system_socket.h>

#include <common/common_module.h>
#include <network/connection_validator.h>

#include "module_information.h"

#include <utils/common/app_info.h>
#include "utils/common/cryptographic_hash.h"
#include <api/global_settings.h>
#include <nx/network/socket_global.h>

#define DEBUG_LOG(MESSAGE) do \
{ \
    if (nx::network::SocketGlobals::debugConfig().moduleFinders) \
        NX_LOGX(MESSAGE, cl_logDEBUG1); \
} while (0)

static const int MAX_CACHE_SIZE_BYTES = 1024 * 64;

namespace {

const unsigned defaultPingTimeoutMs = 1000 * 5;
const unsigned defaultKeepAliveMultiply = 5;
const unsigned errorWaitTimeoutMs = 1000;
const unsigned checkInterfacesTimeoutMs = 60 * 1000;

const QHostAddress defaultModuleRevealMulticastGroup = QHostAddress(lit("239.255.11.11"));
const quint16 defaultModuleRevealMulticastGroupPort = 5007;


} // anonymous namespace

using namespace nx::network;

QnMulticastModuleFinder::QnMulticastModuleFinder(
    bool clientOnly,
    const QHostAddress &multicastGroupAddress,
    const quint16 multicastGroupPort,
    const unsigned int pingTimeoutMillis,
    const unsigned int keepAliveMultiply)
    :
    m_clientMode(clientOnly),
    m_serverSocket(nullptr),
    m_pingTimeoutMillis(pingTimeoutMillis == 0 ? defaultPingTimeoutMs : pingTimeoutMillis),
    m_keepAliveMultiply(keepAliveMultiply == 0 ? defaultKeepAliveMultiply : keepAliveMultiply),
    m_prevPingClock(0),
    m_checkInterfacesTimeoutMs(checkInterfacesTimeoutMs),
    m_lastInterfacesCheckMs(0),
    m_multicastGroupAddress(multicastGroupAddress.isNull() ? defaultModuleRevealMulticastGroup : multicastGroupAddress),
    m_multicastGroupPort(multicastGroupPort == 0 ? defaultModuleRevealMulticastGroupPort : multicastGroupPort),
    m_cachedResponse(MAX_CACHE_SIZE_BYTES)
{
    connect(qnCommon, &QnCommonModule::moduleInformationChanged, this, &QnMulticastModuleFinder::at_moduleInformationChanged, Qt::DirectConnection);
}

QnMulticastModuleFinder::~QnMulticastModuleFinder()
{
    stop();
    clearInterfaces();
}

bool QnMulticastModuleFinder::isValid() const
{
    QnMutexLocker lk(&m_mutex);
    return !m_clientSockets.empty();
}

bool QnMulticastModuleFinder::isDisabled(false);

void QnMulticastModuleFinder::setCheckInterfacesTimeout(unsigned int checkInterfacesTimeoutMs)
{
    m_checkInterfacesTimeoutMs = checkInterfacesTimeoutMs;
}

void QnMulticastModuleFinder::updateInterfaces()
{
    QnMutexLocker lk(&m_mutex);

    QList<QHostAddress> addressesToRemove = m_clientSockets.keys();

    /* This function only adds interfaces to the list. */
    for (const QHostAddress &address : QNetworkInterface::allAddresses())
    {
        addressesToRemove.removeOne(address);

        if (address.protocol() != QAbstractSocket::IPv4Protocol)
            continue;

        if (m_clientSockets.contains(address))
            continue;

        try
        {
            //if( addressToUse == QHostAddress(lit("127.0.0.1")) )
            //    continue;
            auto sock = std::make_unique<nx::network::UDPSocket>(AF_INET);
            sock->bind(SocketAddress(address.toString(), 0));
            sock->getLocalAddress();    //requesting local address. During this call local port is assigned to socket
            sock->setDestAddr(SocketAddress(m_multicastGroupAddress.toString(), m_multicastGroupPort));
            auto it = m_clientSockets.insert(address, sock.release());
            if (m_serverSocket)
            {
                if (!m_serverSocket->joinGroup(m_multicastGroupAddress.toString(), address.toString()))
                {
                    NX_LOGX(lm("Unable to join multicast group %1 on interface %2: %3")
                        .strs(m_multicastGroupAddress, address), cl_logWARNING);
                }
            }

            if (!m_pollSet.add(it.value(), aio::etRead, it.value()))
            {
                const auto error = SystemError::getLastOSErrorText();
                NX_ASSERT(false, Q_FUNC_INFO, error.toUtf8().data());
            }

            NX_LOGX(lm("Joined multicast group %1 on interface %2").strs(
                m_multicastGroupAddress, address), cl_logDEBUG1);
        }
        catch (const std::exception &e)
        {
            NX_LOGX(lm("Failed to create socket on local address %1. %2")
                .strs(address, e.what()), cl_logERROR);
        }
    }

    for (const QHostAddress &address : addressesToRemove)
    {
        UDPSocket *socket = m_clientSockets.take(address);
        m_pollSet.remove(socket, aio::etRead);
        if (m_serverSocket)
            m_serverSocket->leaveGroup(m_multicastGroupAddress.toString(), address.toString());

        NX_LOGX(lm("Left multicast group %1 on interface %2").strs(
            m_multicastGroupAddress, address), cl_logDEBUG1);
    }
}

void QnMulticastModuleFinder::clearInterfaces()
{
    QnMutexLocker lk(&m_mutex);

    if (m_serverSocket)
    {
        for (auto it = m_clientSockets.begin(); it != m_clientSockets.end(); ++it)
            m_serverSocket->leaveGroup(m_multicastGroupAddress.toString(), it.key().toString());

        m_pollSet.remove(m_serverSocket.get(), aio::etRead);
        m_serverSocket.reset();
    }

    for (UDPSocket *socket : m_clientSockets)
    {
        m_pollSet.remove(socket, aio::etRead);
        delete socket;
    }

    m_clientSockets.clear();
    NX_LOGX("Interfaces are cleared", cl_logDEBUG1);
}

void QnMulticastModuleFinder::pleaseStop()
{
    QnLongRunnable::pleaseStop();
    m_pollSet.interrupt();
}

bool QnMulticastModuleFinder::processDiscoveryRequest(UDPSocket *udpSocket)
{
    static const size_t READ_BUFFER_SIZE = UDPSocket::MAX_PACKET_SIZE;
    quint8 readBuffer[READ_BUFFER_SIZE];

    SocketAddress remoteEndpoint;
    int bytesRead = udpSocket->recvFrom(readBuffer, READ_BUFFER_SIZE, &remoteEndpoint);
    if (bytesRead == -1)
    {
        SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
        NX_LOGX(lm("Failed to read socket on local address (%1). %2").strs(
            udpSocket->getLocalAddress(), SystemError::toString(prevErrorCode)), cl_logERROR);
        return false;
    }

    if (!RevealRequest::isValid(readBuffer, readBuffer + bytesRead))
    {
        NX_LOGX(lm("Received invalid request from (%1) on local address %2").strs(
            remoteEndpoint, udpSocket->getLocalAddress()), cl_logDEBUG1);
        return false;
    }

    //TODO #ak RevealResponse class is excess here. Should send/receive QnModuleInformation
    {
        QnMutexLocker lock(&m_moduleInfoMutex);
        if (m_serializedModuleInfo.isEmpty())
            m_serializedModuleInfo = RevealResponse(qnCommon->moduleInformation()).serialize();
    }
    if (!udpSocket->sendTo(m_serializedModuleInfo.data(), m_serializedModuleInfo.size(), remoteEndpoint))
    {
        NX_LOGX(lm("Can't send response to address (%1)").str(remoteEndpoint), cl_logDEBUG1);
        return false;
    };

    DEBUG_LOG(lm("Reveal respose is sent to address (%1)").str(remoteEndpoint));
    return true;
}

void QnMulticastModuleFinder::at_moduleInformationChanged()
{
    QnMutexLocker lock(&m_moduleInfoMutex);
    m_serializedModuleInfo.clear(); // clear cached value
}

RevealResponse *QnMulticastModuleFinder::getCachedValue(const quint8* buffer, const quint8* bufferEnd)
{
    QnCryptographicHash m_mdctx(QnCryptographicHash::Md5);
    m_mdctx.addData(reinterpret_cast<const char*>(buffer), bufferEnd - buffer);
    QByteArray result = m_mdctx.result();
    RevealResponse* response = m_cachedResponse[result];
    if (!response)
    {
        response = new RevealResponse();
        if (!response->deserialize(buffer, bufferEnd))
        {
            delete response;
            return 0;
        }
        m_cachedResponse.insert(result, response, bufferEnd - buffer);
    }
    return response;
}

bool QnMulticastModuleFinder::processDiscoveryResponse(UDPSocket *udpSocket)
{
    const size_t readBufferSize = UDPSocket::MAX_PACKET_SIZE;
    quint8 readBuffer[readBufferSize];

    SocketAddress remoteEndpoint;
    int bytesRead = udpSocket->recvFrom(readBuffer, readBufferSize, &remoteEndpoint);
    if (bytesRead == -1)
    {
        SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
        NX_LOGX(lm("Failed to read response on local address (%1). %2").strs(
            udpSocket->getLocalAddress(), SystemError::toString(prevErrorCode)), cl_logERROR);
        return false;
    }

    RevealResponse *response = getCachedValue(readBuffer, readBuffer + bytesRead);
    if (!response)
    {
        NX_LOGX(lm("Received invalid response from (%1) on local address %2").strs(
            remoteEndpoint, udpSocket->getLocalAddress()), cl_logDEBUG1);
        return false;
    }

    if (response->type != QnModuleInformation::nxMediaServerId()
        && response->type != QnModuleInformation::nxECId())
    {
        DEBUG_LOG(lm("Ignoring %1 (%2) with id %3 on local address %4").strs(
            response->type, remoteEndpoint, response->id, udpSocket->getLocalAddress()));
        return true;
    }

    auto connectionResult = QnConnectionValidator::validateConnection(*response);
    if (connectionResult == Qn::IncompatibleInternalConnectionResult)
    {
        DEBUG_LOG(lm("Ignoring %1 (%2) with different customization %3 on local address %4").strs(
            response->type, remoteEndpoint, response->customization, udpSocket->getLocalAddress()));
        return false;
    }

    if (response->port == 0)
    {
        DEBUG_LOG(lm("Ignoring %1 (%2) with zero port on local address %3").strs(
            response->type, remoteEndpoint, udpSocket->getLocalAddress()));
        return true;
    }

    DEBUG_LOG(lm("Accepting %1 (%2) with id %3 on local address %4").strs(
        response->type, remoteEndpoint, response->id, udpSocket->getLocalAddress()));

    emit responseReceived(
        *response,
        SocketAddress(remoteEndpoint.address.toString(), response->port),
        remoteEndpoint.address);
    return true;
}

void QnMulticastModuleFinder::run()
{
    if (isDisabled)
        return;

    initSystemThreadId();
    NX_LOGX(lit("has started"), cl_logDEBUG1);

    QByteArray revealRequest = RevealRequest::serialize();

    if (!m_clientMode)
    {
        QnMutexLocker lk(&m_mutex);
        m_serverSocket.reset(new UDPSocket(AF_INET));
        m_serverSocket->setReuseAddrFlag(true);
        m_serverSocket->bind(SocketAddress(HostAddress::anyHost, m_multicastGroupPort));
        if (!m_pollSet.add(m_serverSocket.get(), aio::etRead, m_serverSocket.get()))
        {
            const auto error = SystemError::getLastOSErrorText();
            NX_ASSERT(false, Q_FUNC_INFO, error.toUtf8().data());
        }
    }

    while (!needToStop())
    {
        quint64 currentClock = QDateTime::currentMSecsSinceEpoch();

        if (currentClock - m_lastInterfacesCheckMs >= m_checkInterfacesTimeoutMs)
        {
            updateInterfaces();
            m_lastInterfacesCheckMs = currentClock;
        }

        currentClock = QDateTime::currentMSecsSinceEpoch();

        if (currentClock - m_prevPingClock >= m_pingTimeoutMillis)
        {
            if (m_clientMode ||
                (!qnGlobalSettings->isNewSystem() && qnGlobalSettings->isAutoDiscoveryEnabled()))
            {
                QnMutexLocker lk(&m_mutex);
                for (UDPSocket *socket : m_clientSockets)
                {
                    if (!socket->send(revealRequest.data(), revealRequest.size()))
                    {
                        SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
                        NX_LOGX(lm("Failed to send reveal request to %1. %2").strs(
                            socket->getForeignAddress(), SystemError::toString(prevErrorCode)),
                            cl_logDEBUG1);

                        //TODO #ak if corresponding interface is down, should remove socket from set
                    }
                    else
                    {
                        DEBUG_LOG(lm("Reveal request is sent to %1")
                            .strs(socket->getForeignAddress()));
                    }
                }
            }

            m_prevPingClock = currentClock;
        }

        int socketCount = m_pollSet.poll(m_pingTimeoutMillis - (currentClock - m_prevPingClock));

        if (socketCount == 0)
        {
            DEBUG_LOG("poll has timed out");
            continue;
        }

        if (socketCount < 0)
        {
            SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
            if (prevErrorCode == SystemError::interrupted)
                continue;

            NX_LOGX(lit("poll failed. %1").arg(SystemError::toString(prevErrorCode)), cl_logERROR);
            msleep(errorWaitTimeoutMs);
            continue;
        }

        //currentClock = QDateTime::currentMSecsSinceEpoch();

        /* some sockets changed state */
        for (auto it = m_pollSet.begin(); it != m_pollSet.end(); ++it)
        {
            if (!(it.eventType() & aio::etRead))
                continue;

            UDPSocket* udpSocket = static_cast<UDPSocket*>(it.userData());

            if (udpSocket == m_serverSocket.get())
                processDiscoveryRequest(udpSocket);
            else
                processDiscoveryResponse(udpSocket);
        }
    }

    clearInterfaces();

    NX_LOGX(lit("stopped"), cl_logDEBUG1);
}
