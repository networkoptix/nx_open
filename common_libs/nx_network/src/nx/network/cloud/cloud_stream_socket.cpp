/**********************************************************
* Jan 27, 2016
* akolesnikov
***********************************************************/

#include "cloud_stream_socket.h"

#include <utils/common/systemerror.h>

#include <nx/utils/std/future.h>

#include "tunnel/outgoing_tunnel.h"
#include "../socket_global.h"
#include "../system_socket.h"


namespace nx {
namespace network {
namespace cloud {

CloudStreamSocket::CloudStreamSocket(int ipVersion)
:
    m_aioThreadBinder(SocketFactory::createDatagramSocket()),
    m_recvPromisePtr(nullptr),
    m_sendPromisePtr(nullptr),
    m_terminated(false),
    m_ipVersion(ipVersion)
{
    //TODO #ak user MUST be able to bind this object to any aio thread
    //getAioThread binds to an aio thread
    m_socketAttributes.aioThread = m_aioThreadBinder->getAioThread();
}

CloudStreamSocket::~CloudStreamSocket()
{
    // checking that resolution is not in progress
    NX_CRITICAL(
        !nx::network::SocketGlobals::addressResolver().isRequestIdKnown(this),
        "You MUST cancel running async socket operation before "
        "deleting socket if you delete socket from non-aio thread");
}

bool CloudStreamSocket::bind(const SocketAddress& localAddress)
{
    //TODO #ak just ignoring for now. 
        //Usually, we do not care about exact port on tcp client socket
    static_cast<void>(localAddress);
    return true;
}

SocketAddress CloudStreamSocket::getLocalAddress() const
{
    if (m_socketDelegate)
        return m_socketDelegate->getLocalAddress();

    return SocketAddress();
}

bool CloudStreamSocket::close()
{
    shutdown();
    if (m_socketDelegate)
        return m_socketDelegate->close();

    return true;
}

bool CloudStreamSocket::isClosed() const
{
    if (m_socketDelegate)
        return m_socketDelegate->isClosed();

    return true;
}

bool CloudStreamSocket::shutdown()
{
    {
        QnMutexLocker lk(&m_mutex);
        if (m_terminated)
            return true;
        m_terminated = true;
    }

    //interrupting blocking calls
    nx::utils::promise<void> stoppedPromise;
    pleaseStop(
        [this, &stoppedPromise]()
        {
            auto sendPromise = m_sendPromisePtr.exchange(nullptr);
            auto recvPromise = m_recvPromisePtr.exchange(nullptr);
            
            const auto interrupted = std::make_pair(SystemError::interrupted, 0);

            if (sendPromise)
                sendPromise->set_value(interrupted);
            if (recvPromise)
                recvPromise->set_value(interrupted);

            stoppedPromise.set_value();
        });

    stoppedPromise.get_future().wait();
    if (m_socketDelegate)
        m_socketDelegate->shutdown();

    return true;
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
    NX_CRITICAL(!SocketGlobals::aioService().isInAnyAioThread());

    unsigned int sendTimeoutBak = 0;
    if (!getSendTimeout(&sendTimeoutBak))
        return false;
    if (!setSendTimeout(timeoutMillis))
        return false;

    nx::utils::promise<std::pair<SystemError::ErrorCode, size_t>> promise;
    {
        QnMutexLocker lk(&m_mutex);
        if (m_terminated)
        {
            SystemError::setLastErrorCode(SystemError::interrupted);
            return false;
        }
        auto oldPromisePtr = m_sendPromisePtr.exchange(&promise);
        NX_ASSERT(oldPromisePtr == nullptr);
    }

    connectAsync(
        remoteAddress,
        [this, &promise](SystemError::ErrorCode code)
        {
            //to ensure that socket is not used by aio sub-system anymore, we use post
            m_aioThreadBinder->post([this, code](){
                auto promisePtr = m_sendPromisePtr.exchange(nullptr);
                if (promisePtr)
                    promisePtr->set_value(std::make_pair(code, 0));
            });
        });
    
    auto result = promise.get_future().get().first;

    if (result != SystemError::noError)
    {
        SystemError::setLastErrorCode(result);
        return false;
    }

    if (!setSendTimeout(sendTimeoutBak))
        return false;

    return true;
}

int CloudStreamSocket::recv(void* buffer, unsigned int bufferLen, int flags)
{
    NX_CRITICAL(!SocketGlobals::aioService().isInAnyAioThread());

    if (m_socketDelegate)
    {
        {
            QnMutexLocker lk(&m_mutex);
            if (m_terminated)
            {
                SystemError::setLastErrorCode(SystemError::interrupted);
                return 0;
            }
        }

        return m_socketDelegate->recv(buffer, bufferLen, flags);
    }


    SystemError::setLastErrorCode(SystemError::notConnected);
    return -1;
}

int CloudStreamSocket::send(const void* buffer, unsigned int bufferLen)
{
    NX_CRITICAL(!SocketGlobals::aioService().isInAnyAioThread());

    if (m_socketDelegate)
        return m_socketDelegate->send(buffer, bufferLen);

    SystemError::setLastErrorCode(SystemError::notConnected);
    return -1;
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
    auto finalHandler = [this, eventType, handler = move(handler)]() mutable
    {
        if (m_socketDelegate)
        {
            NX_ASSERT(m_aioThreadBinder->getAioThread() == m_socketDelegate->getAioThread());
        }

        m_aioThreadBinder->cancelIOAsync(
            eventType,
            [this, eventType, handler = move(handler)]() mutable
            {
                if (m_socketDelegate)
                    m_socketDelegate->cancelIOSync(eventType);
                handler();
            });
    };

    if (eventType == aio::etWrite || eventType == aio::etNone)
    {
        m_asyncConnectGuard->terminate(); // breaks outgoing connects
        nx::network::SocketGlobals::addressResolver().cancel(
            this, std::move(finalHandler));
    }
    else
    {
        finalHandler();
    }
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
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)> handler)
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
        m_ipVersion,
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

aio::AbstractAioThread* CloudStreamSocket::getAioThread() const
{
    return m_aioThreadBinder->getAioThread();
}

void CloudStreamSocket::bindToAioThread(aio::AbstractAioThread* aioThread)
{
    m_aioThreadBinder->bindToAioThread(aioThread);
    m_socketAttributes.aioThread = aioThread;
}

void CloudStreamSocket::onAddressResolved(
    std::shared_ptr<nx::utils::AsyncOperationGuard::SharedGuard> sharedOperationGuard,
    int remotePort,
    SystemError::ErrorCode osErrorCode,
    std::vector<AddressEntry> dnsEntries)
{
    if (osErrorCode != SystemError::noError)
    {
        NX_LOGX(lm("Address resolve error: %1")
            .arg(SystemError::toString(osErrorCode)), cl_logDEBUG1);
    }

    m_aioThreadBinder->post(
        [sharedOperationGuard = std::move(sharedOperationGuard), remotePort,
            osErrorCode, dnsEntries = std::move(dnsEntries), this]() mutable
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
        });
}

bool CloudStreamSocket::startAsyncConnect(
    std::vector<AddressEntry> dnsEntries,
    int port)
{
    using namespace std::placeholders;

    if (dnsEntries.empty())
    {
        NX_LOGX(lm("No address entry"), cl_logDEBUG1);

        SystemError::setLastErrorCode(SystemError::hostUnreach);
        return false;
    }

    //TODO #ak try every resolved address? Also, should prefer regular address to a cloud one
        //first of all, should check direct
        //then cloud address

    const AddressEntry& dnsEntry = dnsEntries[0];
    switch (dnsEntry.type)
    {
        case AddressType::direct:
            //using tcp connection
            m_socketDelegate.reset(new TCPSocket(true, m_ipVersion));
            setDelegate(m_socketDelegate.get());
            if (!m_socketDelegate->setNonBlockingMode(true))
                return false;
            for (const auto& attr: dnsEntry.attributes)
            {
                if (attr.type == AddressAttributeType::port)
                    port = static_cast<quint16>(attr.value);
            }
            m_socketDelegate->connectAsync(
                SocketAddress(std::move(dnsEntry.host), port),
                std::bind(&CloudStreamSocket::directConnectDone, this, _1));
            return true;

        case AddressType::cloud:
        case AddressType::unknown:  //if peer is unknown, trying to establish cloud connect
        {
            //establishing cloud connect
            unsigned int sendTimeoutMillis = 0;
            if (!getSendTimeout(&sendTimeoutMillis))
                return false;
            auto sharedOperationGuard = m_asyncConnectGuard.sharedGuard();

            // TODO: Need to pass m_ipVersion for IPv6 support
            SocketGlobals::outgoingTunnelPool().establishNewConnection(
                dnsEntry,
                std::chrono::milliseconds(sendTimeoutMillis),
                getSocketAttributes(),
                [this, sharedOperationGuard](
                    SystemError::ErrorCode errorCode,
                    std::unique_ptr<AbstractStreamSocket> cloudConnection)
                {
                    //NOTE: this handler is called from unspecified thread
                    auto operationLock = sharedOperationGuard->lock();
                    if (!operationLock)
                        return; //operation has been cancelled

                    if (errorCode == SystemError::noError)
                    {
                        NX_ASSERT(cloudConnection->getAioThread() == m_aioThreadBinder->getAioThread());
                    }

                    dispatch(
                        [this, errorCode, 
                            cloudConnection = std::move(cloudConnection)]() mutable
                        {
                            onCloudConnectDone(
                                errorCode,
                                std::move(cloudConnection));
                        });
                });
            return true;
        }

        default:
            NX_ASSERT(false);
            SystemError::setLastErrorCode(SystemError::hostUnreach);
            return false;
    }
}

SystemError::ErrorCode CloudStreamSocket::applyRealNonBlockingMode(
    AbstractStreamSocket* streamSocket)
{
    SystemError::ErrorCode errorCode = SystemError::noError;

    if (!m_socketAttributes.nonBlockingMode)
    {
        //restoring default non blocking mode on socket
        if (!streamSocket->setNonBlockingMode(false))
        {
            errorCode = SystemError::getLastOSErrorCode();
            NX_CRITICAL(errorCode != SystemError::noError);
        }
    }

    return errorCode;
}

void CloudStreamSocket::directConnectDone(SystemError::ErrorCode errorCode)
{
    if (errorCode == SystemError::noError)
        errorCode = applyRealNonBlockingMode(m_socketDelegate.get());

    auto userHandler = std::move(m_connectHandler);
    userHandler(errorCode);  //this object can be freed in handler, so using local variable for handler
}

void CloudStreamSocket::onCloudConnectDone(
    SystemError::ErrorCode errorCode,
    std::unique_ptr<AbstractStreamSocket> cloudConnection)
{
    if (errorCode == SystemError::noError)
        errorCode = applyRealNonBlockingMode(cloudConnection.get());

    if (errorCode == SystemError::noError)
    {
        NX_ASSERT(cloudConnection->getAioThread() == m_aioThreadBinder->getAioThread());
        m_socketDelegate = std::move(cloudConnection);
        setDelegate(m_socketDelegate.get());
    }
    else
    {
        NX_ASSERT(!cloudConnection);
    }
    auto userHandler = std::move(m_connectHandler);
    userHandler(errorCode);  //this object can be freed in handler, so using local variable for handler
}

} // namespace cloud
} // namespace network
} // namespace nx
