/**********************************************************
* 30 aug 2013
* a.kolesnikov
***********************************************************/

#include "socket_factory.h"

#include "system_socket.h"
#include "udt/udt_socket.h"
#include "ssl_socket.h"

#include "cloud/cloud_stream_socket.h"
#include "utils/common/cpp14.h"


using namespace nx::network;

std::unique_ptr< AbstractDatagramSocket > SocketFactory::createDatagramSocket()
{
    return std::unique_ptr< AbstractDatagramSocket >(new UDPSocket(false));
}

static std::unique_ptr< AbstractStreamSocket > streamSocket(
        SocketFactory::NatTraversalType nttType,
        SocketFactory::SocketType socketType )
{
    switch( socketType )
    {
        case SocketFactory::SocketType::Default:
            //TODO: #mux NattStreamSocket when it's ready
            // fall into TCP for now

        case SocketFactory::SocketType::Tcp:
            switch( nttType )
            {
                case SocketFactory::NatTraversalType::nttAuto:
                case SocketFactory::NatTraversalType::nttEnabled:
                    return std::make_unique< cloud::CloudStreamSocket >();

                case SocketFactory::NatTraversalType::nttDisabled:
                    return std::make_unique< TCPSocket >( false );

                default:
                    return nullptr;
            }

        case SocketFactory::SocketType::Udt:
            switch( nttType )
            {
                case SocketFactory::NatTraversalType::nttAuto:
                case SocketFactory::NatTraversalType::nttEnabled:
                case SocketFactory::NatTraversalType::nttDisabled:
                    return std::make_unique< UdtStreamSocket >();

                default:
                    return nullptr;
            }
            break;

        default:
            return nullptr;
    };
}

namespace {
SocketFactory::CreateStreamSocketFuncType createStreamSocketFunc;
SocketFactory::CreateStreamServerSocketFuncType createStreamServerSocketFunc;
}

std::unique_ptr< AbstractStreamSocket > SocketFactory::createStreamSocket(
    bool sslRequired,
    SocketFactory::NatTraversalType natTraversalRequired )
{
    if (createStreamSocketFunc)
        return createStreamSocketFunc(sslRequired, natTraversalRequired);

    auto result = streamSocket( natTraversalRequired,
                                s_enforcedStreamSocketType );

#ifdef ENABLE_SSL
    if( result && sslRequired )
        result.reset( new QnSSLSocket( result.release(), false ) );
#endif
    
    return std::move( result );
}

static std::unique_ptr< AbstractStreamServerSocket > streamServerSocket(
        SocketFactory::NatTraversalType nttType,
        SocketFactory::SocketType socketType )
{
    static_cast< void >( nttType );
    switch( socketType )
    {
        case SocketFactory::SocketType::Default:
            //TODO: #mux NattStreamSocket when it's ready
            // fall into TCP for now

        case SocketFactory::SocketType::Tcp:
            return std::make_unique< TCPServerSocket >();

        case SocketFactory::SocketType::Udt:
            return std::make_unique< UdtStreamServerSocket >();

        default:
            return nullptr;
    };
}

std::unique_ptr< AbstractStreamServerSocket > SocketFactory::createStreamServerSocket(
    bool sslRequired,
    SocketFactory::NatTraversalType natTraversalRequired )
{
    if (createStreamServerSocketFunc)
        return createStreamServerSocketFunc(sslRequired, natTraversalRequired);

    auto result = streamServerSocket( natTraversalRequired,
                                      s_enforcedStreamSocketType );

#ifdef ENABLE_SSL
    if( result && sslRequired )
        result.reset( new SSLServerSocket( result.release(), true ) );
#endif // ENABLE_SSL

    return std::move( result );
}

void SocketFactory::enforceStreamSocketType( SocketType type )
{
    QString typeStr;
    switch (type)
    {
        case SocketType::Default:   typeStr = lit( "Default" ); break;
        case SocketType::Tcp:       typeStr = lit( "TCP" );     break;
        case SocketType::Udt:       typeStr = lit( "UDT" );     break;
    }

    s_enforcedStreamSocketType = type;
    qWarning() << ">>> SocketFactory::enforceStreamSocketType("
               << typeStr << ") <<<";
}

bool SocketFactory::isStreamSocketTypeEnforced()
{
    return s_enforcedStreamSocketType != SocketType::Default;
}

SocketFactory::CreateStreamSocketFuncType 
    SocketFactory::setCreateStreamSocketFunc(
        CreateStreamSocketFuncType newFactoryFunc)
{
    auto bak = std::move(createStreamSocketFunc);
    createStreamSocketFunc = std::move(newFactoryFunc);
    return bak;
}

SocketFactory::CreateStreamServerSocketFuncType
    SocketFactory::setCreateStreamServerSocketFunc(
        CreateStreamServerSocketFuncType newFactoryFunc)
{
    auto bak = std::move(createStreamServerSocketFunc);
    createStreamServerSocketFunc = std::move(newFactoryFunc);
    return bak;
}

std::atomic< SocketFactory::SocketType >
    SocketFactory::s_enforcedStreamSocketType(
        SocketFactory::SocketType::Default );
