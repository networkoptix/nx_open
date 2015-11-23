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
class SocketFactory
{
public:
    enum class NatTraversalType
    {
        nttAuto,
        nttEnabled,
        nttDisabled
    };

    static std::unique_ptr< AbstractDatagramSocket > createDatagramSocket(
            NatTraversalType natTraversalRequired = NatTraversalType::nttAuto );

    /*!
        \param sslRequired If \a true than it is guaranteed that returned object can be safely cast to \a AbstractEncryptedStreamSocket
    */
    static std::unique_ptr< AbstractStreamSocket > createStreamSocket(
        bool sslRequired = false,
        NatTraversalType natTraversalRequired = NatTraversalType::nttAuto );

    static std::unique_ptr< AbstractStreamServerSocket > createStreamServerSocket(
        bool sslRequired = false,
        NatTraversalType natTraversalRequired = NatTraversalType::nttAuto );

    enum class SocketType
    {
        Default,    ///< production mode
        Tcp,        ///< \class TcpSocket and \class TcpServerSocket
        Udt,        ///< \class UdtSocket and \class UdtServerSocket
    };

    /*! Enforces factory to produce certain sockets
     *  \note DEBUG use ONLY! */
    static void enforceStreamSocketType( SocketType type );
    static bool isStreamSocketTypeEnforced();

private:
    SocketFactory();
    SocketFactory( const SocketFactory& );
    SocketFactory& operator=( const SocketFactory& );

    ~SocketFactory();

    static std::atomic< SocketType > s_enforcedStreamSocketType;
};

#endif  //SOCKET_FACTORY_H
