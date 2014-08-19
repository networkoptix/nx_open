/**********************************************************
* 30 aug 2013
* a.kolesnikov
***********************************************************/

#ifndef SOCKET_FACTORY_H
#define SOCKET_FACTORY_H

#include "abstract_socket.h"


//!Contains factory methods for creating sockets
/*!
    All factory methods return object created new operator \a new
*/
class SocketFactory
{
public:
    // TODO: #Elric #enum
    enum NatTraversalType
    {
        nttAuto,
        nttEnabled,
        //!Using this value causes TCP protocol to be used for stream-orientied sockets
        nttDisabled
    };

    static AbstractDatagramSocket* createDatagramSocket();
    /*!
        \param sslRequired If \a true than it is guaranteed that returned object can be safely cast to \a AbstractEncryptedStreamSocket
    */
    static AbstractStreamSocket* createStreamSocket( bool sslRequired = false, NatTraversalType natTraversalRequired = nttAuto );
    static AbstractStreamServerSocket* createStreamServerSocket( bool sslRequired = false, NatTraversalType natTraversalRequired = nttAuto );

private:
    SocketFactory();
    SocketFactory( const SocketFactory& );
    SocketFactory& operator=( const SocketFactory& );

    ~SocketFactory();
};

#endif  //SOCKET_FACTORY_H
