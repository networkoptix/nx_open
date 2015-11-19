/**********************************************************
* 30 aug 2013
* a.kolesnikov
***********************************************************/

#include "socket_factory.h"

#include "mixed_tcp_udt_server_socket.h"
#include "system_socket.h"
#include "udt_socket.h"
#include "ssl_socket.h"

#include "utils/common/cpp14.h"

std::unique_ptr< AbstractDatagramSocket > SocketFactory::createDatagramSocket(
    NatTraversalType natTraversalRequired )
{
    switch( natTraversalRequired )
    {
        case NatTraversalType::nttAuto:
        case NatTraversalType::nttEnabled:
            return std::unique_ptr< AbstractDatagramSocket >( new UDPSocket( true ) );

        case NatTraversalType::nttDisabled:
            return std::unique_ptr< AbstractDatagramSocket >( new UDPSocket( false ) );
    };

    return std::unique_ptr< AbstractDatagramSocket >();
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
                    return std::make_unique< TCPSocket >( true );

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
                    return std::make_unique< UdtStreamSocket >( true );

                case SocketFactory::NatTraversalType::nttDisabled:
                    return std::make_unique< UdtStreamSocket >( false );

                default:
                    return nullptr;
            }
            break;

        default:
            return nullptr;
    };
}

std::unique_ptr< AbstractStreamSocket > SocketFactory::createStreamSocket(
    bool sslRequired,
    SocketFactory::NatTraversalType natTraversalRequired )
{
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
    auto result = streamServerSocket( natTraversalRequired,
                                      s_enforcedStreamSocketType );

#ifdef ENABLE_SSL
    if( result && sslRequired )
        result.reset( new SSLServerSocket( result.release(), false ) );
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

std::atomic< SocketFactory::SocketType >
    SocketFactory::s_enforcedStreamSocketType(
        SocketFactory::SocketType::Default );
