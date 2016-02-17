/**********************************************************
* Jan 27, 2016
* akolesnikov
***********************************************************/

#include "cloud_stream_socket.h"

#include <future>

#include <utils/common/systemerror.h>

#include "tunnel/outgoing_tunnel.h"
#include "../socket_global.h"
#include "../system_socket.h"


namespace nx {
namespace network {
namespace cloud {

CloudStreamSocket::CloudStreamSocket()
:
    m_aioThreadBinder(SocketFactory::createDatagramSocket()),
    m_recvPromisePtr(nullptr),
    m_sendPromisePtr(nullptr)
{
    //getAioThread binds to an aio thread
    m_socketAttributes.aioThread = m_aioThreadBinder->getAioThread();
}

CloudStreamSocket::~CloudStreamSocket()
{
}

bool CloudStreamSocket::bind(const SocketAddress& localAddress)
{
    //TODO #ak just ignoring for now. 
        //Usually, we do not care about exact port on tcp client socket
    return true;
}

SocketAddress CloudStreamSocket::getLocalAddress() const
{
    if (m_socketDelegate)
        return m_socketDelegate->getLocalAddress();

    return SocketAddress();
}

void CloudStreamSocket::close()
{
    if (m_socketDelegate)
        m_socketDelegate->close();
}

bool CloudStreamSocket::isClosed() const
{
    if (m_socketDelegate)
        return m_socketDelegate->isClosed();

    return true;
}

void CloudStreamSocket::shutdown()
{
    //interrupting blocking calls
    pleaseStop(
        [this](){
            if (m_recvPromisePtr.load())
            {
                m_recvPromisePtr.load()->set_value(
                    std::make_pair(SystemError::interrupted, 0));
                m_recvPromisePtr.store(nullptr);
            }

            if (m_sendPromisePtr.load())
            {
                m_sendPromisePtr.load()->set_value(
                    std::make_pair(SystemError::interrupted, 0));
                m_sendPromisePtr.store(nullptr);
            }
        });
    if (m_socketDelegate)
        m_socketDelegate->shutdown();
}

AbstractSocket::SOCKET_HANDLE CloudStreamSocket::handle() const
{
    if (m_socketDelegate)
        return m_socketDelegate->handle();

    SystemError::setLastErrorCode(SystemError::notSupported);
    return -1;
}

bool CloudStreamSocket::reopen()
{
    if (m_socketDelegate)
        return m_socketDelegate->reopen();

    return false;
}

bool CloudStreamSocket::connect(
    const SocketAddress& remoteAddress,
    unsigned int timeoutMillis)
{
    std::promise<SystemError::ErrorCode> promise;
    connectAsync(
        remoteAddress,
        [this, &promise](SystemError::ErrorCode code)
        {
            //to ensure that socket is not used by aio sub-system anymore, we use post
            m_aioThreadBinder->post([code, &promise](){
                promise.set_value(code);
            });
        });

    auto result = promise.get_future().get();
    if (result != SystemError::noError)
    {
        SystemError::setLastErrorCode(result);
        return false;
    }

    return true;
}

int CloudStreamSocket::recv(void* buffer, unsigned int bufferLen, int flags)
{
    Buffer tmpBuffer;
    tmpBuffer.reserve(bufferLen);

    int totallyRead = 0;
    do
    {
        //TODO #ak (#CLOUD-209) timeout is processed incorrectly since it is applied to every recvImpl call

        const auto lastRead = recvImpl(&tmpBuffer);
        if (lastRead <= 0)
            return totallyRead;

        memcpy(static_cast<char*>(buffer) + totallyRead, tmpBuffer.data(), lastRead);
        totallyRead += lastRead;
    }
    while ((flags & MSG_WAITALL) && (totallyRead < bufferLen));

    return totallyRead;
}

int CloudStreamSocket::send(const void* buffer, unsigned int bufferLen)
{
    std::promise<std::pair<SystemError::ErrorCode, size_t>> promise;
    nx::Buffer sendBuffer = nx::Buffer::fromRawData(
        static_cast<const char*>(buffer),
        bufferLen);
    m_sendPromisePtr.store(&promise);
    sendAsync(
        sendBuffer,
        [&promise, this](SystemError::ErrorCode code, size_t size)
        {
            m_aioThreadBinder->post(
                [this, code, size, &promise]()
                {
                    promise.set_value(std::make_pair(code, size));
                    m_sendPromisePtr.store(nullptr);
                });
        });

    //sendAsync handles timeout properly
    auto result = promise.get_future().get();
    if (result.first != SystemError::noError)
    {
        SystemError::setLastErrorCode(result.first);
        return -1;
    }

    return result.second;
}

SocketAddress CloudStreamSocket::getForeignAddress() const
{
    if (m_socketDelegate)
        return m_socketDelegate->getForeignAddress();

    return SocketAddress();
}

bool CloudStreamSocket::isConnected() const
{
    if (m_socketDelegate)
        return m_socketDelegate->isConnected();

    return false;
}

void CloudStreamSocket::cancelIOAsync(
    aio::EventType eventType,
    nx::utils::MoveOnlyFunc<void()> handler)
{
    if (eventType == aio::etWrite || eventType == aio::etNone)
    {
        m_asyncConnectGuard->terminate(); // breaks outgoing connects
        nx::network::SocketGlobals::addressResolver().cancel(this);
    }

    if (m_socketDelegate)
    {
        assert(m_aioThreadBinder->getAioThread() == m_socketDelegate->getAioThread());
    }

    m_aioThreadBinder->cancelIOAsync(
        eventType,
        [this, eventType, handler = move(handler)]() mutable
        {
            if (m_socketDelegate)
                m_socketDelegate->cancelIOSync(eventType);
            handler();
        });
}

void CloudStreamSocket::cancelIOSync(aio::EventType eventType)
{
    if (eventType == aio::etWrite || eventType == aio::etNone)
    {
        m_asyncConnectGuard->terminate(); // breaks outgoing connects
        nx::network::SocketGlobals::addressResolver().cancel(this);
    }
    m_aioThreadBinder->cancelIOSync(eventType);
    if (m_socketDelegate)
        m_socketDelegate->cancelIOSync(eventType);
}

void CloudStreamSocket::post(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_aioThreadBinder->post(std::move(handler));
}

void CloudStreamSocket::dispatch(nx::utils::MoveOnlyFunc<void()> handler)
{
    m_aioThreadBinder->dispatch(std::move(handler));
}

void CloudStreamSocket::connectAsync(
    const SocketAddress& address,
    std::function<void(SystemError::ErrorCode)> handler)
{
    m_connectHandler = std::move(handler);

    using namespace std::placeholders;
    auto sharedOperationGuard = m_asyncConnectGuard.sharedGuard();
    const auto remotePort = address.port;
    nx::network::SocketGlobals::addressResolver().resolveAsync(
        address.address,
        std::bind(
            &CloudStreamSocket::onAddressResolved,
            this, sharedOperationGuard, remotePort, _1, _2),
        true,
        this);
}

void CloudStreamSocket::readSomeAsync(
    nx::Buffer* const buf,
    std::function<void(SystemError::ErrorCode, size_t)> handler)
{
    if (m_socketDelegate)
        m_socketDelegate->readSomeAsync(buf, std::move(handler));
    else
        m_aioThreadBinder->post(std::bind(handler, SystemError::notConnected, 0));
}

void CloudStreamSocket::sendAsync(
    const nx::Buffer& buf,
    std::function<void(SystemError::ErrorCode, size_t)> handler)
{
    if (m_socketDelegate)
        m_socketDelegate->sendAsync(buf, std::move(handler));
    else
        m_aioThreadBinder->post(std::bind(handler, SystemError::notConnected, 0));
}

void CloudStreamSocket::registerTimer(
    std::chrono::milliseconds timeoutMs,
    nx::utils::MoveOnlyFunc<void()> handler)
{
    m_aioThreadBinder->registerTimer(timeoutMs, std::move(handler));
}

aio::AbstractAioThread* CloudStreamSocket::getAioThread()
{
    return m_aioThreadBinder->getAioThread();
}

void CloudStreamSocket::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    m_aioThreadBinder->bindToAioThread(aioThread);
}

void CloudStreamSocket::onAddressResolved(
    std::shared_ptr<nx::utils::AsyncOperationGuard::SharedGuard> sharedOperationGuard,
    int remotePort,
    SystemError::ErrorCode osErrorCode,
    std::vector<AddressEntry> dnsEntries)
{
    auto operationLock = sharedOperationGuard->lock();
    if (!operationLock)
        return; //operation has been cancelled

    if (osErrorCode != SystemError::noError)
    {
        auto connectHandlerBak = std::move(m_connectHandler);
        connectHandlerBak(osErrorCode);
        return;
    }

    if (!startAsyncConnect(std::move(dnsEntries), remotePort))
    {
        auto connectHandlerBak = std::move(m_connectHandler);
        connectHandlerBak(SystemError::getLastOSErrorCode());
    }
}

bool CloudStreamSocket::startAsyncConnect(
    std::vector<AddressEntry> dnsEntries,
    int port)
{
    if (dnsEntries.empty())
    {
        SystemError::setLastErrorCode(SystemError::hostUnreach);
        return false;
    }

    //TODO #ak try every resolved address? Also, should prefer regular address to a cloud one
    const AddressEntry& dnsEntry = dnsEntries[0];
    switch (dnsEntry.type)
    {
        case AddressType::regular:
            //using tcp connection
            m_socketDelegate.reset(new TCPSocket(true));
            setDelegate(m_socketDelegate.get());
            if (!m_socketDelegate->setNonBlockingMode(true))
                return false;
            for (const auto& attr: dnsEntry.attributes)
            {
                if (attr.type == AddressAttributeType::nxApiPort)
                    port = static_cast<quint16>(attr.value);
            }
            m_socketDelegate->connectAsync(
                SocketAddress(std::move(dnsEntry.host), port),
                m_connectHandler);
            return true;

        case AddressType::cloud:
        case AddressType::unknown:  //if peer is unknown, trying to establish cloud connect
        {
            unsigned int sockSendTimeout = 0;
            if (!getSendTimeout(&sockSendTimeout))
                return false;

            //establishing cloud connect
            unsigned int sendTimeoutMillis = 0;
            if (!getSendTimeout(&sendTimeoutMillis))
                return false;
            auto sharedOperationGuard = m_asyncConnectGuard.sharedGuard();
            SocketGlobals::outgoingTunnelPool().establishNewConnection(
                dnsEntry,
                std::chrono::milliseconds(sendTimeoutMillis),
                getSocketAttributes(),
                [this, sharedOperationGuard](
                    SystemError::ErrorCode errorCode,
                    std::unique_ptr<AbstractStreamSocket> cloudConnection)
                {
                    auto operationLock = sharedOperationGuard->lock();
                    if (!operationLock)
                        return; //operation has been cancelled
                    onCloudConnectDone(
                        errorCode,
                        std::move(cloudConnection));
                });
        }

        default:
            assert(false);
            SystemError::setLastErrorCode(SystemError::hostUnreach);
            return false;
    }
}

int CloudStreamSocket::recvImpl(nx::Buffer* const buf)
{
    std::promise<std::pair<SystemError::ErrorCode, size_t>> promise;
    m_recvPromisePtr.store(&promise);
    readSomeAsync(
        buf,
        [this, &promise](SystemError::ErrorCode code, size_t size)
        {
            m_aioThreadBinder->post(
                [this, code, size, &promise]()
                {
                    promise.set_value(std::make_pair(code, size));
                    m_recvPromisePtr.store(nullptr);
                });
        });

    auto result = promise.get_future().get();
    if (result.first != SystemError::noError)
    {
        SystemError::setLastErrorCode(result.first);
        return -1;
    }

    return result.second;
}

void CloudStreamSocket::onCloudConnectDone(
    SystemError::ErrorCode errorCode,
    std::unique_ptr<AbstractStreamSocket> cloudConnection)
{
    if (errorCode == SystemError::noError)
    {
        m_socketDelegate = std::move(cloudConnection);
        assert(cloudConnection->getAioThread() == m_aioThreadBinder->getAioThread());
        setDelegate(m_socketDelegate.get());
    }
    else
    {
        assert(!cloudConnection);
    }
    auto userHandler = std::move(m_connectHandler);
    userHandler(errorCode);  //this object can be freed in handler, so using local variable for handler
}

} // namespace cloud
} // namespace network
} // namespace nx
