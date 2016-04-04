#ifndef NX_CC_DNS_TABLE_H
#define NX_CC_DNS_TABLE_H

#include <nx/utils/thread/mutex.h>
#include <nx/network/dns_resolver.h>
#include <utils/common/guard.h>

#include "cdb_endpoint_fetcher.h"
#include "mediator_connections.h"


//!Types used in resolving peer names
/*!
    \note It is not a DNS implementation! It is something proprietary used to
        get network address of NX peer
*/

namespace nx {
namespace network {
namespace cloud {

enum class AddressType
{
    unknown,
    direct, //!< Address for direct simple connection
    cloud, //!< Address that requires mediator (e.g. hole punching)
};

QString toString(const AddressType& type);

enum class AddressAttributeType
{
    unknown,
    port, //!< NX peer port
};

enum class CloudConnectType
{
    kUnknown,
    kUdtHp,      // UDT over UDP hole punching
    kTcpHp,      // TCP hole punching
    kProxy,      // Proxy server address
};

struct NX_NETWORK_API AddressAttribute
{
    AddressAttributeType type;
    quint64 value; // TODO: boost::variant when int is not enough

    AddressAttribute(AddressAttributeType type_, quint64 value_ = 0);
    bool operator ==(const AddressAttribute& rhs) const;
    bool operator <(const AddressAttribute& rhs) const;
    QString toString() const;
};

struct NX_NETWORK_API AddressEntry
{
    AddressType type;
    HostAddress host;
    std::vector<AddressAttribute> attributes;

    AddressEntry(
        AddressType type_ = AddressType::unknown,
        HostAddress host_ = HostAddress());

    AddressEntry(const SocketAddress& address);

    bool operator ==(const AddressEntry& rhs) const;
    bool operator <(const AddressEntry& rhs) const;
    QString toString() const;
};

//!Contains peer names, their known addresses and some attributes
class NX_NETWORK_API AddressResolver
        : public QnStoppableAsync
{
public:
    AddressResolver(
        std::shared_ptr<hpm::api::MediatorClientTcpConnection> mediatorConnection);

    //!Add new peer address
    /*!
        Peer addresses are resolved from time to time in the following way:\n
        - custom NX resolve request is sent if \a nxApiPort attribute is provided.
            If peer responds and reports same name as \a peerName than address
            considered "resolved"
        - if host does not respond to custom NX resolve request or no
            \a nxApiPort attribute, than ping is used from time to time
        - resolved address is "resolved" for only some period of time

        \param attributes Attributes refer to \a hostAddress not \a peerName

        \note Peer can have multiple addresses
    */
    void addFixedAddress(
        const HostAddress& hostName, const SocketAddress& hostAddress);

    //!Removes added address
    void removeFixedAddress(
        const HostAddress& hostName, const SocketAddress& hostAddress);

    typedef std::pair<HostAddress, AddressType> TypedAddres;

    static QString toString(const TypedAddres& address);

    //!Resolves domain address to the list of subdomains
    /*!
        \example resolveDomain( domain ) = { sub1.domain, sub2.domain, ... }

        \note \a handler might be called within this function in case if
            values are avaliable from cache
     */
    void resolveDomain(
        const HostAddress& domain,
        utils::MoveOnlyFunc<void(std::vector<TypedAddres>)> handler );

    typedef utils::MoveOnlyFunc<void(
        SystemError::ErrorCode, std::vector<AddressEntry>)> ResolveHandler;

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
    void resolveAsync(
        const HostAddress& hostName, ResolveHandler handler,
        bool natTraversal = true, void* requestId = nullptr);

    std::vector<AddressEntry> resolveSync(
        const HostAddress& hostName, bool natTraversal);

    //!Cancels request
    /*!
        if \a handler not provided the method will block until actual
        cancelation is done
    */
    void cancel(
        void* requestId,
        nx::utils::MoveOnlyFunc<void()> handler = nullptr);

    bool isRequestIdKnown(void* requestId) const;

    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

private:
    struct HostAddressInfo
    {
        enum class State { unresolved, resolved, inProgress };

        std::vector<AddressEntry> fixedEntries;
        std::set<void*> pendingRequests;

        HostAddressInfo();

        State dnsState() { return m_dnsState; }
        void dnsProgress() { m_dnsState = State::inProgress; }
        void setDnsEntries(std::vector<AddressEntry> entries = {});

        State mediatorState() { return m_mediatorState; }
        void mediatorProgress() { m_mediatorState = State::inProgress; }
        void setMediatorEntries(std::vector< AddressEntry > entries = {});

        void checkExpirations();
        bool isResolved(bool natTraversal = false) const;
        std::vector<AddressEntry> getAll() const;

    private:
        State m_dnsState;
        std::chrono::system_clock::time_point m_dnsResolveTime;
        std::vector<AddressEntry> m_dnsEntries;

        State m_mediatorState;
        std::chrono::system_clock::time_point m_mediatorResolveTime;
        std::vector<AddressEntry> m_mediatorEntries;
    };

    typedef std::map< HostAddress, HostAddressInfo > HostInfoMap;
    typedef HostInfoMap::iterator HaInfoIterator;

    struct RequestInfo
    {
        const HostAddress address;
        bool inProgress;
        bool natTraversal;
        ResolveHandler handler;
        Guard guard;

        RequestInfo(RequestInfo&&);
        RequestInfo(
            HostAddress _address, bool _natTraversal, ResolveHandler _handler);
    };

    void tryFastDomainResolve(HaInfoIterator info);
    void dnsResolve(HaInfoIterator info);
    void mediatorResolve(HaInfoIterator info, QnMutexLockerBase* lk);

    std::vector<Guard> grabHandlers(
        SystemError::ErrorCode lastErrorCode,
        HaInfoIterator info, QnMutexLockerBase* lk );

private:
    mutable QnMutex m_mutex;
    mutable QnWaitCondition m_condition;
    HostInfoMap m_info;
    std::multimap<void*, RequestInfo> m_requests;

    DnsResolver m_dnsResolver;
    std::shared_ptr<hpm::api::MediatorClientTcpConnection> m_mediatorConnection;
};

} // namespace cloud
} // namespace network
} // namespace nx

#endif // NX_CC_DNS_TABLE_H
