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

static std::unique_ptr< AbstractStreamSocket > defaultStreamSocketFactoryFunc(
    SocketFactory::NatTraversalType nttType,
    SocketFactory::SocketType forcedSocketType)
{
    switch (forcedSocketType)
    {
        case SocketFactory::SocketType::cloud:
            switch (nttType)
            {
                case SocketFactory::NatTraversalType::nttAuto:
                case SocketFactory::NatTraversalType::nttEnabled:
                    //return std::make_unique< TCPSocket >(true);
                    //return std::make_unique< cloud::CloudStreamSocket >();

                case SocketFactory::NatTraversalType::nttDisabled:
                    return std::make_unique< TCPSocket >( false );
            }

        case SocketFactory::SocketType::tcp:
            return std::make_unique< TCPSocket >(nttType != SocketFactory::NatTraversalType::nttDisabled);

        case SocketFactory::SocketType::udt:
            return std::make_unique< UdtStreamSocket >();

        default:
            return nullptr;
    };
}


static std::unique_ptr< AbstractStreamServerSocket > defaultStreamServerSocketFactoryFunc(
    SocketFactory::NatTraversalType nttType,
    SocketFactory::SocketType socketType)
{
    static_cast< void >(nttType);
    switch (socketType)
    {
        case SocketFactory::SocketType::cloud:
            // TODO #mux: uncomment when works properly
            // return std::make_unique< cloud::CloudServerSocket >();

        case SocketFactory::SocketType::tcp:
            return std::make_unique< TCPServerSocket >();

        case SocketFactory::SocketType::udt:
            return std::make_unique< UdtStreamServerSocket >();

        default:
            return nullptr;
    };
}

namespace {
SocketFactory::CreateStreamSocketFuncType createStreamSocketFunc;
SocketFactory::CreateStreamServerSocketFuncType createStreamServerSocketFunc;
}

std::unique_ptr< AbstractDatagramSocket > SocketFactory::createDatagramSocket()
{
    return std::unique_ptr< AbstractDatagramSocket >(new UDPSocket(false));
}

std::unique_ptr< AbstractStreamSocket > SocketFactory::createStreamSocket(
    bool sslRequired,
    SocketFactory::NatTraversalType natTraversalRequired )
{
    if (createStreamSocketFunc)
        return createStreamSocketFunc(sslRequired, natTraversalRequired);

    auto result = defaultStreamSocketFactoryFunc(
        natTraversalRequired,
        s_enforcedStreamSocketType);

#ifdef ENABLE_SSL
    if( result && sslRequired )
        result.reset( new QnSSLSocket( result.release(), false ) );
#endif
    
    return std::move( result );
}

std::unique_ptr< AbstractStreamServerSocket > SocketFactory::createStreamServerSocket(
    bool sslRequired,
    SocketFactory::NatTraversalType natTraversalRequired )
{
    if (createStreamServerSocketFunc)
        return createStreamServerSocketFunc(sslRequired, natTraversalRequired);

    auto result = defaultStreamServerSocketFactoryFunc(
        natTraversalRequired,
        s_enforcedStreamSocketType);

#ifdef ENABLE_SSL
    if( result && sslRequired )
        result.reset( new SSLServerSocket( result.release(), true ) );
#endif // ENABLE_SSL

    return std::move( result );
}


QString SocketFactory::toString( SocketType type )
{
    switch ( type )
    {
        case SocketType::cloud: return lit( "cloud" );
        case SocketType::tcp: return lit( "tcp" );
        case SocketType::udt: return lit( "udt" );
    }

    NX_ASSERT( false, lm("Unrecognized socket type: ").arg(static_cast<int>(type)) );
    return QString();
}

SocketFactory::SocketType SocketFactory::stringToSocketType( QString type )
{
    if( type.toLower() == lit("cloud") ) return SocketType::cloud;
    if( type.toLower() == lit("tcp") ) return SocketType::tcp;
    if( type.toLower() == lit("udt") ) return SocketType::udt;

    NX_ASSERT( false, lm("Unrecognized socket type: ").arg(type) );
    return SocketType::cloud;
}

void SocketFactory::enforceStreamSocketType( SocketType type )
{
    s_enforcedStreamSocketType = type;
    qWarning() << ">>> SocketFactory::enforceStreamSocketType("
               << toString( type ) << ") <<<";
}

void SocketFactory::enforceStreamSocketType( QString type )
{
    enforceStreamSocketType( stringToSocketType( type ) );
}

bool SocketFactory::isStreamSocketTypeEnforced()
{
    return s_enforcedStreamSocketType != SocketType::cloud;
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

// TODO: Change to Cloud when avaliable
std::atomic< SocketFactory::SocketType >
    SocketFactory::s_enforcedStreamSocketType(
        SocketFactory::SocketType::cloud );
