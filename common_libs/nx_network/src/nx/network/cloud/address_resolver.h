#pragma once

#include <set>
#include <deque>

#include <nx/network/async_stoppable.h>
#include <nx/network/dns_resolver.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/scope_guard.h>

namespace nx {
namespace network {
namespace cloud {

enum class AddressType
{
    unknown,
    direct, /**< Address for direct simple connection. */
    cloud, /**< Address that requires mediator (e.g. hole punching). */
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
    /** NX peer port. */
    port,
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

/**
 * Resolves hostname to IP address or cloud host id which
 * is enough to establish connection to the host.
 */
class NX_NETWORK_API AddressResolver:
    public QnStoppableAsync
{
public:
    AddressResolver();
    virtual ~AddressResolver() = default;

    /**
     * Add new peer address.
     * Peer addresses are resolved from time to time in the following way:\n
     * - Custom NX resolve request is sent if nxApiPort attribute is provided.
     *   If peer responds and reports same name as peerName than address
     *   considered "resolved".
     * - If host does not respond to custom NX resolve request or no
     *   nxApiPort attribute, than ping is used from time to time.
     * - Resolved address is "resolved" for only some period of time.
     *
     * @param attributes Attributes refer to hostAddress not peerName.
     *
     * NOTE: Peer can have multiple addresses.
     */
    void addFixedAddress(
        const HostAddress& hostName, const SocketAddress& endpoint);

    /**
     * Removes added address, if endpoint is boost::none, removes all addresses.
     */
    void removeFixedAddress(
        const HostAddress& hostName,
        boost::optional<SocketAddress> endpoint = boost::none);

    typedef utils::MoveOnlyFunc<void(
        SystemError::ErrorCode, std::deque<AddressEntry>)> ResolveHandler;

    /**
     * Resolves hostName like DNS server does.
     * handler is called with complete address list includung:
     * - Addresses reported by AddressResolver::addFixedAddress.
     * - Resolve result from DnsResolver.
     * - Resolve result from MediatorAddressResolver (TODO :).
     *
     * @param natTraversalSupport defines if mediator should be used for address resolution.
     *
     * NOTE: Handler might be called within this function in case if
     *   values are avaliable from cache.
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

    /**
     * Cancels request.
     * If handler not provided the method will block until actual
     *   cancelation is done.
     */
    void cancel(
        void* requestId,
        nx::utils::MoveOnlyFunc<void()> handler = nullptr);

    bool isRequestIdKnown(void* requestId) const;

    bool isCloudHostName(const QString& hostName) const;

    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

    DnsResolver& dnsResolver() { return m_dnsResolver; }

    /** @return true If endpoint is valid for socket connect. */
    bool isValidForConnect(const SocketAddress& endpoint) const;

    void setCloudResolveEnabled(bool isEnabled);
    bool isCloudResolveEnabled() const;

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

    void tryFastDomainResolve(HaInfoIterator info);

    void dnsResolve(
        HaInfoIterator info, QnMutexLockerBase* lk, bool needMediator, int ipVersion);

    void mediatorResolve(
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
    bool m_isCloudResolveEnabled = false;

    DnsResolver m_dnsResolver;
    const QRegExp m_cloudAddressRegExp;

    bool isCloudHostName(
        QnMutexLockerBase* const lk,
        const QString& hostName) const;
};

} // namespace cloud
} // namespace network
} // namespace nx
