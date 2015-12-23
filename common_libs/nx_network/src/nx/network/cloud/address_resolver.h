#ifndef NX_CC_DNS_TABLE_H
#define NX_CC_DNS_TABLE_H

#include <utils/common/guard.h>
#include <nx/utils/thread/mutex.h>
#include <nx/network/dns_resolver.h>

#include "cdb_endpoint_fetcher.h"
#include "mediator_connections.h"

//!Types used in resolving peer names
/*!
    \note It is not a DNS implementation! It is something proprietary used to get network address of NX peer
*/
namespace nx {
namespace network {
namespace cloud {

enum class AddressType
{
    unknown,
    regular,    //!< IP address for direct simple connection
    cloud,      //!< Address that requires mediator (e.g. hole punching)
};

QString toString( const AddressType& type );

enum class AddressAttributeType
{
    peerFoundPassively, //!< NX peer reported its address and name by itself
    nxApiPort,          //!< NX peer (mediaserver) port
};

struct NX_NETWORK_API AddressAttribute
{
    AddressAttributeType type;
    quint64 value; // TODO: boost::variant ?

    AddressAttribute( AddressAttributeType type_, quint64 value_ );
    bool operator ==( const AddressAttribute& rhs ) const;
    QString toString() const;
};

struct NX_NETWORK_API AddressEntry
{
    AddressType type;
    HostAddress host;
    std::vector< AddressAttribute > attributes;

    AddressEntry( AddressType type_ = AddressType::unknown,
                  HostAddress host_ = HostAddress() );
    bool operator ==( const AddressEntry& rhs ) const;
    QString toString() const;
};

//!Contains peer names, their known addresses and some attributes
class NX_NETWORK_API AddressResolver
        : public QnStoppableAsync
{
public:
    AddressResolver(std::shared_ptr<MediatorClientConnection> mediatorConnection);

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

    typedef std::function< void( SystemError::ErrorCode,
                                 std::vector< AddressEntry > ) > ResolveHandler;

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
    void resolveAsync( const HostAddress& hostName, ResolveHandler handler,
                       bool natTraversal, void* requestId = nullptr );

    std::vector< AddressEntry > resolveSync( const HostAddress& hostName, bool natTraversal );

    //!Cancels request
    /*!
        if \a handler not provided the method will block until actual
        cancelation is done
    */
    void cancel( void* requestId, std::function< void() > handler = nullptr );
    bool isRequestIdKnown( void* requestId ) const;

    void pleaseStop( std::function<void()> handler ) override;

private:
    struct HostAddressInfo
    {
        enum class State { unresolved, resolved, inProgress };

        std::vector< AddressEntry > peers;
        std::set< void* > pendingRequests;

        HostAddressInfo();

        State dnsState() { return m_dnsState; }
        void dnsProgress() { m_dnsState = State::inProgress; }
        void setDnsEntries( std::vector< AddressEntry > entries
                                = std::vector< AddressEntry >() );

        State mediatorState() { return m_mediatorState; }
        void mediatorProgress() { m_mediatorState = State::inProgress; }
        void setMediatorEntries( std::vector< AddressEntry > entries
                                    = std::vector< AddressEntry >() );

        void checkExpirations();
        bool isResolved( bool natTraversal ) const;
        std::vector< AddressEntry > getAll() const;

    private:
        State m_dnsState;
        std::chrono::system_clock::time_point m_dnsResolveTime;
        std::vector< AddressEntry > m_dnsEntries;

        State m_mediatorState;
        std::chrono::system_clock::time_point m_mediatorResolveTime;
        std::vector< AddressEntry > m_mediatorEntries;
    };

    typedef std::map< HostAddress, HostAddressInfo > HaInfoMap;
    typedef HaInfoMap::iterator HaInfoIterator;

    struct RequestInfo
    {
        const HostAddress address;
        bool inProgress;
        bool natTraversal;
        ResolveHandler handler;
        Guard guard;

        RequestInfo( HostAddress _address,
                     bool _natTraversal,
                     ResolveHandler _handler );
        RequestInfo(RequestInfo&&);
    };

    void dnsResolve( HaInfoIterator info );
    void mediatorResolve( HaInfoIterator info, QnMutexLockerBase* lk );

    std::vector< Guard > grabHandlers( SystemError::ErrorCode lastErrorCode,
                                       HaInfoIterator info );

private:
    mutable QnMutex m_mutex;
    mutable QnWaitCondition m_condition;
    HaInfoMap m_info;
    std::multimap< void*, RequestInfo > m_requests;

    DnsResolver m_dnsResolver;
    std::shared_ptr<MediatorClientConnection> m_mediatorConnection;
};

} // namespace cloud
} // namespace network
} // namespace nx

#endif // NX_CC_DNS_TABLE_H
