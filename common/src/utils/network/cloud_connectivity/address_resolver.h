#ifndef NX_CC_DNS_TABLE_H
#define NX_CC_DNS_TABLE_H

#include "utils/common/guard.h"
#include "utils/thread/mutex.h"

#include "utils/network/dns_resolver.h"
#include "utils/network/stun/async_client.h"

// Forward
namespace nx { class SocketGlobals; }

//!Types used in resolving peer names
/*!
    \note It is not a DNS implementation! It is something proprietary used to get network address of NX peer
*/
namespace nx {
namespace cc {

enum class AddressType
{
    unknown,
    regular,    //!< IP address for direct simple connection
    cloud,      //!< Address that requires mediator (e.g. hole punching)
};

enum class AddressAttributeType
{
    peerFoundPassively, //!< NX peer reported its address and name by itself
    nxApiPort,          //!< NX peer (mediaserver) port
};

struct AddressAttribute
{
    AddressAttributeType type;
    quint64 value; // TODO: boost::variant ?

    AddressAttribute( AddressAttributeType type_, quint64 value_ );
    bool operator ==( const AddressAttribute& rhs ) const;
};

struct AddressEntry
{
    AddressType type;
    HostAddress host;
    std::vector< AddressAttribute > attributes;

    AddressEntry( AddressType type_ = AddressType::unknown,
                  HostAddress host_ = HostAddress() );
    bool operator ==( const AddressEntry& rhs ) const;
};

//!Contains peer names, their known addresses and some attributes
class AddressResolver
{
    AddressResolver();
    friend class ::nx::SocketGlobals;

public:
    //!Add new peer address
    /*!
        Peer addresses are resolved from time to time in the following way:\n
        - custom NX resolve request is sent if \a nxApiPort attribute is provided. If peer responds and reports same name as \a peerName than address considered "resolved"
        - if host does not respond to custom NX resolve request or no \a nxApiPort attribute, than ping is used from time to time
        - resolved address is "resolved" for only some period of time

        \param attributes Attributes refer to \a hostAddress not \a peerName

        \note Peer can have multiple addresses
    */
    void addPeerAddress( const HostAddress& peerName, HostAddress hostAddress,
                         std::vector< AddressAttribute > attributes );

    //!Resolves hostName like DNS server does
    /*!
        \a handler is called with complete address list includung:
            - addresses reported by \fn addPeerAddress
            - resolve result from \class DnsResolver
            - resolve result from \class MediatorAddressResolver (TODO :)

        \note \a handler might be called within this function in case if
            values are avaliable from cache

        \a natTraversal defines if mediator should be used for address resolution
    */
    bool resolveAsync( const HostAddress& hostName,
                       std::function< void( std::vector< AddressEntry > ) > handler,
                       bool natTraversal, void* requestId = nullptr );

    std::vector< AddressEntry > resolveSync( const HostAddress& hostName, bool natTraversal );
    void cancel( void* requestId, bool waitForRunningHandlerCompletion = true );
    bool isRequestIdKnown( void* requestId ) const;

private:
    struct HostAddressInfo
    {
        enum class State { unresolved, resolved, inProgress };

        // TODO: add expiration times
        std::vector< AddressEntry > peers;

        State dnsState;
        boost::optional< AddressEntry > dnsResult;

        State mediatorState;
        std::vector< AddressEntry > mediatorResult;

        std::set< void* > pendingRequests;

        HostAddressInfo();
        bool isResolved( bool natTraversal ) const;
        std::vector< AddressEntry > getAll() const;
    };

    typedef std::map< HostAddress, HostAddressInfo > HaInfoMap;
    typedef HaInfoMap::iterator HaInfoIterator;

    struct RequestInfo
    {
        const HostAddress& address;
        bool inProgress;
        bool natTraversal;
        std::function< void( std::vector< AddressEntry > ) > handler;

        RequestInfo( const HostAddress& _address, bool _natTraversal,
                     std::function< void( std::vector< AddressEntry > ) > _handler );
    };

    bool dnsResolve( HaInfoIterator info, QnMutexLockerBase* lk );
    bool mediatorResolve( HaInfoIterator info, QnMutexLockerBase* lk );

    std::vector< Guard > grabHandlers( HaInfoIterator info );

private:
    mutable QnMutex m_mutex;
    mutable QnWaitCondition m_condition;
    HaInfoMap m_info;
    std::multimap< void*, RequestInfo > m_requests;

    DnsResolver m_dnsResolver;
    stun::AsyncClient m_stunClient;
};

} // namespace cc
} // namespace nx

#endif // NX_CC_DNS_TABLE_H
