// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <deque>
#include <optional>
#include <set>

#include <nx/network/async_stoppable.h>
#include <nx/network/dns_resolver.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/scope_guard.h>

namespace nx::network {

class AbstractResolver;
class CloudAddressResolver;

/**
 * Resolves hostname to IP address or cloud host id which
 * is enough to establish connection to the host.
 */
class NX_NETWORK_API AddressResolver:
    public QnStoppableAsync
{
public:
    static constexpr auto kDnsCacheTimeout = std::chrono::seconds(10);
    static constexpr auto kMediatorCacheTimeout = std::chrono::seconds(1);

    using ResolveHandler = utils::MoveOnlyFunc<void(
        SystemError::ErrorCode, std::deque<AddressEntry>)>;

    AddressResolver();
    virtual ~AddressResolver() = default;

    // TODO: #akolesnikov Refactor "const HostAddress& hostName" into std::string.

    /**
     * Tells the resolver to resolve hostName into the target.
     */
    void addFixedAddress(
        const HostAddress& hostName, const HostAddress& target);

    /**
     * Tells the resolver to resolve hostName into the endpoint. This means the resulting
     * AddressEntry will have endpoint.host resolve result and endpoint.port as an attribute.
     * See AddressEntry structure for details on attributes.
     */
    void addFixedAddress(
        const HostAddress& hostName, const SocketAddress& endpoint);

    /**
     * Removes address added with AddressResolver::addFixedAddress.
     * If endpoint is std::nullopt then removes all addresses.
     */
    void removeFixedAddress(
        const HostAddress& hostName,
        std::optional<SocketAddress> endpoint = std::nullopt);

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

    bool isCloudHostname(const std::string_view& hostname) const;
    bool isCloudHostname(const QString& hostname) const;
    bool isCloudHostname(const char* hostname) const;

    void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override;

    DnsResolver& dnsResolver() { return m_dnsResolver; }

    /** @return true If endpoint is valid for socket connect. */
    bool isValidForConnect(const SocketAddress& endpoint) const;

    /**
     * By default, kDnsCacheTimeout.
     */
    void setDnsCacheTimeout(std::chrono::milliseconds timeout);

    /**
     * By default, kMediatorCacheTimeout.
     */
    void setMediatorCacheTimeout(std::chrono::milliseconds timeout);

protected:
    struct NX_NETWORK_API HostAddressInfo
    {
        explicit HostAddressInfo(
            bool _isLikelyCloudAddress,
            std::chrono::milliseconds dnsCacheTimeout,
            std::chrono::milliseconds mediatorCacheTimeout);

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
        const std::chrono::milliseconds m_dnsCacheTimeout;

        State m_mediatorState;
        std::chrono::steady_clock::time_point m_mediatorResolveTime;
        std::vector<AddressEntry> m_mediatorEntries;
        const std::chrono::milliseconds m_mediatorCacheTimeout;
    };

    using HostInfoMap = std::map<
        std::tuple<int /*ipVersion*/, HostAddress, NatTraversalSupport>,
        HostAddressInfo>;

    using HaInfoIterator = HostInfoMap::iterator;

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
        const std::string& hostName,
        NatTraversalSupport natTraversalSupport,
        int ipVersion,
        ResolveResult* resolved);

    void dnsResolve(
        HaInfoIterator info, nx::Locker<nx::Mutex>* lk, bool needMediator, int ipVersion);

    void mediatorResolve(
        HaInfoIterator info, nx::Locker<nx::Mutex>* lk, bool needDns, int ipVersion);

    std::vector<nx::utils::Guard> grabHandlers(
        SystemError::ErrorCode lastErrorCode, HaInfoIterator info);

protected:
    mutable nx::Mutex m_mutex;
    mutable nx::WaitCondition m_condition;
    HostInfoMap m_info;
    std::multimap<void*, RequestInfo> m_requests;
    bool m_isCloudResolveEnabled = false;
    std::chrono::milliseconds m_dnsCacheTimeout = kDnsCacheTimeout;
    std::chrono::milliseconds m_mediatorCacheTimeout = kMediatorCacheTimeout;

    DnsResolver m_dnsResolver;

    std::vector<std::unique_ptr<AbstractResolver>> m_nonBlockingResolvers;
    CloudAddressResolver* m_cloudAddressResolver = nullptr;
    PredefinedHostResolver* m_predefinedHostResolver = nullptr;
};

} // namespace nx::network
