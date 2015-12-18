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
    unsigned int timeoutMillis )
{
    //TODO #ak use timeoutMillis

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
    const SocketAddress& addr,
    std::function<void( SystemError::ErrorCode )>&& handler )
{
    //TODO #ak use socket write timeout
    m_connectHandler = std::move(handler);

    const auto remotePort = addr.port;
    nx::SocketGlobals::addressResolver().resolveAsync(
        addr.address,
        [this, remotePort](
            SystemError::ErrorCode osErrorCode,
            std::vector<AddressEntry> dnsEntries)
        {
            if (osErrorCode != SystemError::noError)
            {
                auto connectHandlerBak = std::move(m_connectHandler);
                connectHandlerBak(osErrorCode);
                return;
            }

            if (!startAsyncConnect(std::move(dnsEntries), remotePort))
            {
                auto connectHandlerBak = std::move( m_connectHandler );
                connectHandlerBak( SystemError::getLastOSErrorCode() );
            }
        },
        true,
        this);   //TODO #ak resolve cancellation
}

void CloudStreamSocket::applyCachedAttributes()
{
    //TODO #ak applying cached attributes to socket m_socketDelegate
}

void CloudStreamSocket::onResolveDone( std::vector<AddressEntry> dnsEntries )
{
    //TODO #ak which port shell we actually use???
    int port = 0;
    if( !startAsyncConnect( std::move( dnsEntries ), port ) )
    {
        auto connectHandlerBak = std::move( m_connectHandler );
        connectHandlerBak( SystemError::getLastOSErrorCode() );
    }
}

bool CloudStreamSocket::startAsyncConnect(
    std::vector<AddressEntry>&& dnsEntries,
    int port )
{
    if( dnsEntries.empty() )
    {
        SystemError::setLastErrorCode( SystemError::hostUnreach );
        return false;
    }

    std::shared_ptr<CloudTunnel> cloudTunnel;
    for (const auto dnsEntry : dnsEntries)
    {
        if (dnsEntry.type == AddressType::regular)
        {
            /*
            const auto enforcedPort = dnsEntry.attributes.find(
                        AddressAttributeType::nxApiPort);
            if (enforcedPort != dnsEntry.attributes.end())
                port = enforcedPort->second.value;
            */

            m_socketDelegate.reset( new TCPSocket(true) );
            applyCachedAttributes();
            m_socketDelegate->connectAsync(
                SocketAddress(std::move(dnsEntry.host), port),
                m_connectHandler );

            return true;
        }
        else
        {
            //if (!cloudTunnel)
                //cloudTunnel = CloudTonelPool::getTunnel(address.host);

            cloudTunnel->addAddress(dnsEntry);
        }
    }

    unsigned int sockSendTimeout = 0;
    if( !getSendTimeout( &sockSendTimeout ) )
        return false;

    cloudTunnel->connect(sockSendTimeout, [this](
        SystemError::ErrorCode errorCode,
        std::unique_ptr<AbstractStreamSocket> cloudConnection)
    {
        if( errorCode == SystemError::noError )
        {
            applyCachedAttributes();
        }

        auto userHandler = std::move(m_connectHandler);
        //this object can be freed in handler, so using local variable for handler
        userHandler( errorCode);
    });

    return true;
}

} // namespace cc
} // namespace nx
