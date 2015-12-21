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
                if (code == SystemError::noError)
                    if (startAsyncConnect(std::move(address), std::move(entries)))
                        return;
                    else
                        code = SystemError::hostUnreach;

                auto connectHandlerBak = std::move(m_connectHandler);
                lk.unlock();
                connectHandlerBak(code);
                return;
            }
        });
}

bool CloudStreamSocket::startAsyncConnect(const SocketAddress& originalAddress,
                                          std::vector<AddressEntry> dnsEntries)
{
    std::vector<CloudConnectType> cloudConnectTypes;
    for (const auto entry : dnsEntries)
    {
        switch( entry.type )
        {
            case AddressType::regular:
            {
                SocketAddress target(entry.host, originalAddress.port);
                for(const auto& attr : entry.attributes)
                    if(attr.type == AddressAttributeType::nxApiPort)
                        target.port = static_cast<quint16>(attr.value);

                m_socketDelegate.reset(new TCPSocket(true));
                if (!m_socketOptions->apply(m_socketDelegate.get()))
                    return false;

                m_socketDelegate->connectAsync(
                    std::move(target), std::move(m_connectHandler));

                return true;
            }
            case AddressType::cloud:
            {
                auto ccType = CloudConnectType::unknown;
                for(const auto& attr : entry.attributes)
                    if(attr.type == AddressAttributeType::cloudConnect)
                        ccType = static_cast<CloudConnectType>(attr.value);

                cloudConnectTypes.push_back(ccType);
                break;
            }
            default:
                Q_ASSERT_X(false, Q_FUNC_INFO, "Unexpected AddressType value!");

        };
    }

    if (cloudConnectTypes.empty())
        return false;

    auto cloudTunnel = SocketGlobals::tunnelPool().get(
        originalAddress.address.toString().toUtf8(), std::move(cloudConnectTypes));

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

    return true;
}

} // namespace cc
} // namespace nx
