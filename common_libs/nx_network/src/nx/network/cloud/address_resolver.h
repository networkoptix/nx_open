#pragma once

#include <set>
#include <deque>

#include <nx/network/dns_resolver.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/scope_guard.h>

#include "mediator_client_connections.h"

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

struct TypedAddress
{
    HostAddress address;
    AddressType type;

    TypedAddress(HostAddress address_, AddressType type_);
    QString toString() const;
};

enum class AddressAttributeType
{
    unknown,
    port, //!< NX peer port
};

enum class CloudConnectType
{
    unknown = 0,
    forwardedTcpPort = 0x01,   /**< E.g., Upnp */
    udpHp = 0x02,      /**< UDP hole punching */
    tcpHp = 0x04,      /**< TCP hole punching */
    proxy = 0x08,      /**< Proxy server address */
    all = forwardedTcpPort | udpHp | tcpHp | proxy,
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
    typedef hpm::api::MediatorClientTcpConnection MediatorConnection;
    AddressResolver(std::unique_ptr<MediatorConnection> mediatorConnection);
    virtual ~AddressResolver() = default;

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
        const HostAddress& hostName, const SocketAddress& endpoint);

    //!Removes added address, if endpoint is boost::none, removes all addresses.
    void removeFixedAddress(
        const HostAddress& hostName, boost::optional<SocketAddress> endpoint = boost::none);

    //!Resolves domain address to the list of subdomains
    /*!
        \example resolveDomain( domain ) = { sub1.domain, sub2.domain, ... }

        \note \a handler might be called within this function in case if
            values are avaliable from cache
     */
    void resolveDomain(
        const HostAddress& domain,
        utils::MoveOnlyFunc<void(std::vector<TypedAddress>)> handler);

    typedef utils::MoveOnlyFunc<void(
        SystemError::ErrorCode, std::deque<AddressEntry>)> ResolveHandler;

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
        const HostAddress& hostName,
        ResolveHandler handler,
        NatTraversalSupport natTraversalSupport,
        int ipVersion,
        void* requestId = nullptr);

    std::deque<AddressEntry> resolveSync(
        const HostAddress& hostName,
        NatTraversalSupport natTraversalSupport,
        int ipVersion);

    //!Cancels request
    /*!
        if \a handler not provided the method will block until actual
        cancelation is done
    */
    void cancel(
        void* requestId,
        nx::utils::MoveOnlyFunc<void()> handler = nullptr);

    bool isRequestIdKnown(void* requestId) const;

    bool isCloudHostName(const QString& hostName) const;

    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

    /**
     * true: resolve on mediator to have a chanse to get direct IPs, DNS is used in case if
     *      mediator returned nothing.
     * false: do not resolve on mediator, set cloud address in case of patter match pattern match,
     *      DNS is involved only if pattern does not match.
     */
    static const bool kResolveOnMediator = false;

    DnsResolver& dnsResolver() { return m_dnsResolver; }

    /** @return true If endpoint is valid for socket connect. */
    bool isValidForConnect(const SocketAddress& endpoint) const;

protected:
    struct NX_NETWORK_API HostAddressInfo
    {
        explicit HostAddressInfo(bool _isLikelyCloudAddress);

        const bool isLikelyCloudAddress;
        std::vector<AddressEntry> fixedEntries;
        std::set<void*> pendingRequests;

        enum class State { unresolved, resolved, inProgress };

        State dnsState() const { return m_dnsState; }
        void dnsProgress() { m_dnsState = State::inProgress; }
        void setDnsEntries(std::vector<AddressEntry> entries = {});

        State mediatorState() const { return m_mediatorState; }
        void mediatorProgress() { m_mediatorState = State::inProgress; }
        void setMediatorEntries(std::vector< AddressEntry > entries = {});

        void checkExpirations();
        bool isResolved(NatTraversalSupport natTraversalSupport) const;
        std::deque<AddressEntry> getAll() const;

    private:
        State m_dnsState;
        std::chrono::steady_clock::time_point m_dnsResolveTime;
        std::vector<AddressEntry> m_dnsEntries;

        State m_mediatorState;
        std::chrono::steady_clock::time_point m_mediatorResolveTime;
        std::vector<AddressEntry> m_mediatorEntries;
    };

    typedef std::map< HostAddress, HostAddressInfo > HostInfoMap;
    typedef HostInfoMap::iterator HaInfoIterator;

    struct RequestInfo
    {
        const HostAddress address;
        bool inProgress;
        NatTraversalSupport natTraversalSupport;
        ResolveHandler handler;
        Guard guard;

        RequestInfo(
            HostAddress address,
            NatTraversalSupport natTraversalSupport,
            ResolveHandler handler);
    };

    virtual bool isMediatorAvailable() const;

    void tryFastDomainResolve(HaInfoIterator info);

    void dnsResolve(
        HaInfoIterator info, QnMutexLockerBase* lk, bool needMediator, int ipVersion);

    void mediatorResolve(
        HaInfoIterator info, QnMutexLockerBase* lk, bool needDns, int ipVersion);

    void mediatorResolveImpl(
        HaInfoIterator info, QnMutexLockerBase* lk, bool needDns, int ipVersion);

    std::vector<Guard> grabHandlers(
        SystemError::ErrorCode lastErrorCode, HaInfoIterator info);

    template<typename Functor>
    bool iterateSubdomains(const QString& domain, const Functor& functor)
    {
        // TODO: #mux Think about better representation to increase performance
        const QString suffix = lm(".%1").arg(domain);
        for (auto it = m_info.begin(); it != m_info.end(); ++it)
        {
            if (it->first.toString().endsWith(suffix) &&
                !it->second.getAll().empty())
            {
                if (functor(it))
                    return true;
            }
        }

        return false;
    }

protected:
    mutable QnMutex m_mutex;
    mutable QnWaitCondition m_condition;
    HostInfoMap m_info;
    std::multimap<void*, RequestInfo> m_requests;

    DnsResolver m_dnsResolver;
    std::unique_ptr<MediatorConnection> m_mediatorConnection;
    const QRegExp m_cloudAddressRegExp;

    bool isCloudHostName(
        QnMutexLockerBase* const lk,
        const QString& hostName) const;
};

} // namespace cloud
} // namespace network
} // namespace nx
