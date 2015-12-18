#include "cloud_stream_socket.h"

#include <future>

#include <utils/common/systemerror.h>

#include "cloud_tunnel.h"
#include "../socket_global.h"
#include "../system_socket.h"


namespace nx {
namespace cc {

bool CloudStreamSocket::connect(
    const SocketAddress& remoteAddress,
    unsigned int timeoutMillis)
{
    //TODO #ak use timeoutMillis in wait condition

    //issuing async call and waiting for completion
    std::promise<SystemError::ErrorCode> completionPromise;
    auto asyncRequestCompletionResult = completionPromise.get_future();
    auto completionHandler = [&completionPromise]( SystemError::ErrorCode errorCode ) mutable {
        completionPromise.set_value( errorCode );
    };
    connectAsyncImpl( remoteAddress, completionHandler );
    asyncRequestCompletionResult.wait();
    if( asyncRequestCompletionResult.get() == SystemError::noError )
        return true;
    SystemError::setLastErrorCode( asyncRequestCompletionResult.get() );
    return false;
}

void CloudStreamSocket::connectAsyncImpl(
    const SocketAddress& address,
    std::function<void(SystemError::ErrorCode)>&& handler)
{
    m_connectHandler = std::move(handler);

    auto sharedGuard = m_asyncGuard.sharedGuard();
    nx::SocketGlobals::addressResolver().resolveAsync(
        address.address,
        [this, address, sharedGuard](SystemError::ErrorCode code,
                                     std::vector<AddressEntry> entries)
        {
            if (auto lk = sharedGuard->lock())
            {
                if (entries.empty() && code == SystemError::noError)
                    code = SystemError::hostUnreach;

                if (code != SystemError::noError)
                {
                    auto connectHandlerBak = std::move(m_connectHandler);
                    lk.unlock();
                    connectHandlerBak(code);
                    return;
                }

                startAsyncConnect(std::move(address), std::move(entries));
            }
        });
}

void CloudStreamSocket::startAsyncConnect(const SocketAddress& originalAddress,
                                          std::vector<AddressEntry> dnsEntries)
{
    std::shared_ptr<CloudTunnel> cloudTunnel;
    for (const auto dnsEntry : dnsEntries)
    {
        if (dnsEntry.type == AddressType::regular)
        {
            auto port = originalAddress.port;
            /* TODO: #mux fix this code
             *
            const auto enforcedPort = dnsEntry.attributes.find(
                        AddressAttributeType::nxApiPort);
            if (enforcedPort != dnsEntry.attributes.end())
                port = enforcedPort->second.value;
            */

            m_socketDelegate.reset(new TCPSocket(true));
            m_socketOptions->apply(m_socketDelegate.get());
            return m_socketDelegate->connectAsync(
                SocketAddress(std::move(dnsEntry.host), port),
                std::move(m_connectHandler));
        }

        if (!cloudTunnel)
            cloudTunnel = SocketGlobals::cloudTunnelPool().getTunnel(
                originalAddress.address.toString().toUtf8());

        cloudTunnel->addAddress(dnsEntry);
    }

    auto sharedGuard = m_asyncGuard.sharedGuard();
    cloudTunnel->connect( m_socketOptions,
                          [this, sharedGuard](
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractStreamSocket> socket)
    {
        if (auto lk = sharedGuard->lock())
        {
            m_socketDelegate = std::move(socket);
            const auto handler = std::move(m_connectHandler);

            lk.unlock();
            handler(errorCode);
        }
    });
}

} // namespace cc
} // namespace nx
