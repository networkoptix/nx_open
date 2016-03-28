/**********************************************************
* 30 aug 2013
* a.kolesnikov
***********************************************************/

#ifndef SOCKET_FACTORY_H
#define SOCKET_FACTORY_H

#include <atomic>

#include "abstract_socket.h"

//!Contains factory methods for creating sockets
/*!
    All factory methods return object created new operator \a new
*/
class NX_NETWORK_API SocketFactory
{
public:
    enum class SocketType
    {
        cloud,    ///< production mode
        tcp,        ///< \class TcpSocket and \class TcpServerSocket
        udt,        ///< \class UdtSocket and \class UdtServerSocket
    };

    enum class NatTraversalType
    {
        nttAuto,
        nttEnabled,
        nttDisabled
    };

    typedef std::function<std::unique_ptr< AbstractStreamSocket >(
        bool /*sslRequired*/,
        NatTraversalType /*natTraversalRequired*/)> CreateStreamSocketFuncType;
    typedef std::function<std::unique_ptr< AbstractStreamServerSocket >(
        bool /*sslRequired*/,
        NatTraversalType /*natTraversalRequired*/)> CreateStreamServerSocketFuncType;


    static std::unique_ptr< AbstractDatagramSocket > createDatagramSocket();

    /*!
        \param sslRequired If \a true than it is guaranteed that returned object can be safely cast to \a AbstractEncryptedStreamSocket
    */
    static std::unique_ptr< AbstractStreamSocket > createStreamSocket(
        bool sslRequired = false,
        NatTraversalType natTraversalRequired = NatTraversalType::nttAuto );

    static std::unique_ptr< AbstractStreamServerSocket > createStreamServerSocket(
        bool sslRequired = false,
        NatTraversalType natTraversalRequired = NatTraversalType::nttAuto );

    static QString toString( SocketType type );
    static SocketType stringToSocketType( QString type );

    /*! Enforces factory to produce certain sockets
     *  \note DEBUG use ONLY! */
    static void enforceStreamSocketType( SocketType type );
    static void enforceStreamSocketType( QString type );
    static bool isStreamSocketTypeEnforced();

    /** Sets new factory. Returns old one */
    static CreateStreamSocketFuncType 
        setCreateStreamSocketFunc(CreateStreamSocketFuncType newFactoryFunc);
    static CreateStreamServerSocketFuncType
        setCreateStreamServerSocketFunc(CreateStreamServerSocketFuncType newFactoryFunc);

private:
    SocketFactory();
    SocketFactory( const SocketFactory& );
    SocketFactory& operator=( const SocketFactory& );

    ~SocketFactory();

    static std::atomic< SocketType > s_enforcedStreamSocketType;
};

#endif  //SOCKET_FACTORY_H
