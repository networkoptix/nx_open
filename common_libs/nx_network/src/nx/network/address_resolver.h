#pragma once

#include <set>
#include <deque>

#include <nx/network/async_stoppable.h>
#include <nx/network/dns_resolver.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/scope_guard.h>

namespace nx {
namespace network {

class AbstractResolver;

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
     * Peer addresses are resolved from time to time in the following way:
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
     * Removes address added with AddressResolver::addFixedAddress.
     * If endpoint is boost::none then removes all addresses.
     */
    void removeFixedAddress(
        const HostAddress& hostName,
        boost::optional<SocketAddress> endpoint = boost::none);

    typedef utils::MoveOnlyFunc<void(
        SystemError::ErrorCode, std::deque<AddressEntry>)> ResolveHandler;

    /**
     * Resolves hostName like DNS server does.
     * Handler is called with complete address list includung:
     * - Addresses reported by AddressResolver::addFixedAddress.
     * - Resolve result from DnsResolver.
     * - Resolve result from MediatorAddressResolver.
     *
     * @param natTraversalSupport defines if mediator should be used for address resolution.
     *
     * NOTE: Handler might be called within this function in case if
     *     values are avaliable from cache.
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

    typedef std::map<
        std::tuple<int /*ipVersion*/, HostAddress>,
        HostAddressInfo> HostInfoMap;
    typedef HostInfoMap::iterator HaInfoIterator;

    struct RequestInfo
    {
        const HostAddress address;
        bool inProgress;
        NatTraversalSupport natTraversalSupport;
        ResolveHandler handler;
        nx::utils::Guard guard;

        RequestInfo(
            HostAddress address,
            NatTraversalSupport natTraversalSupport,
            ResolveHandler handler);
    };

    bool resolveNonBlocking(
        const QString& hostName,
        NatTraversalSupport natTraversalSupport,
        int ipVersion,
        std::deque<AddressEntry>* resolvedAddresses);

    void dnsResolve(
        HaInfoIterator info, QnMutexLockerBase* lk, bool needMediator, int ipVersion);

    void mediatorResolve(
        HaInfoIterator info, QnMutexLockerBase* lk, bool needDns, int ipVersion);

    std::vector<nx::utils::Guard> grabHandlers(
        SystemError::ErrorCode lastErrorCode, HaInfoIterator info);

protected:
    mutable QnMutex m_mutex;
    mutable QnWaitCondition m_condition;
    HostInfoMap m_info;
    std::multimap<void*, RequestInfo> m_requests;
    bool m_isCloudResolveEnabled = false;

    DnsResolver m_dnsResolver;
    const QRegExp m_cloudAddressRegExp;

    std::vector<std::unique_ptr<AbstractResolver>> m_nonBlockingResolvers;
    PredefinedHostResolver* m_predefinedHostResolver = nullptr;

    bool isCloudHostName(
        QnMutexLockerBase* const lk,
        const QString& hostName) const;
};

} // namespace network
} // namespace nx
