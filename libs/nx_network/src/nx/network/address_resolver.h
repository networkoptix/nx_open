// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <deque>
#include <optional>
#include <set>

#include <nx/network/async_stoppable.h>
#include <nx/network/dns_resolver.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/thread/mutex.h>

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

    using ResolveHandler = nx::utils::MoveOnlyFunc<void(
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
     * Handler is called with complete address list including:
     * - Addresses reported by AddressResolver::addFixedAddress.
     * - Resolve result from DnsResolver.
     * - Resolve result from MediatorAddressResolver.
     *
     * @param natTraversalSupport defines if mediator should be used for address resolution.
     *
     * NOTE: Handler might be called within this function in case if
     *     values are available from cache.
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
     *   cancellation is done.
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

    /**
     * By default, kDnsCacheTimeout.
     */
    void setDnsCacheTimeout(std::chrono::milliseconds timeout);

protected:
    struct NX_NETWORK_API HostAddressInfo
    {
        explicit HostAddressInfo(
            std::chrono::milliseconds dnsCacheTimeout);

        std::set<void*> pendingRequests;

        enum class State { unresolved, resolved, inProgress };

        State dnsState() const { return m_dnsState; }
        void dnsProgress() { m_dnsState = State::inProgress; }
        void setDnsEntries(std::deque<AddressEntry> entries = {});

        void checkExpirations();
        bool isResolved() const;
        std::deque<AddressEntry> getAll() const;

    private:
        State m_dnsState = State::unresolved;
        std::chrono::steady_clock::time_point m_dnsResolveTime;
        std::optional<std::deque<AddressEntry>> m_dnsEntries;
        const std::chrono::milliseconds m_dnsCacheTimeout;
    };

    struct ResolveTaskKey
    {
        int ipVersion = AF_INET;
        std::string hostname;

        std::string toString() const;

        // TODO: #akolesnikov replace with operator<=> after upgrading compiler.
        bool operator<(const ResolveTaskKey&) const;
    };

    using HostInfoMap = std::map<ResolveTaskKey, HostAddressInfo>;

    using HaInfoIterator = HostInfoMap::iterator;

    struct RequestInfo
    {
        const std::string hostname;
        ResolveHandler handler;
        bool handlerAboutToBeInvoked = false;
        nx::utils::Guard guard;

        RequestInfo(std::string hostname, ResolveHandler handler);
    };

    bool resolveNonBlocking(
        const std::string& hostName,
        NatTraversalSupport natTraversalSupport,
        int ipVersion,
        ResolveResult* resolved);

    void dnsResolve(
        HaInfoIterator info, nx::Locker<nx::Mutex>* lk, int ipVersion);

    void reportResult(
        HaInfoIterator info,
        SystemError::ErrorCode lastErrorCode,
        std::deque<AddressEntry> entries);

    std::vector<std::multimap<void*, RequestInfo>::iterator>
        selectRequestsToComplete(const nx::Locker<nx::Mutex>&, HaInfoIterator info);

protected:
    mutable nx::Mutex m_mutex;
    HostInfoMap m_dnsCache;
    std::multimap<void*, RequestInfo> m_requests;
    std::chrono::milliseconds m_dnsCacheTimeout = kDnsCacheTimeout;

    DnsResolver m_dnsResolver;

    std::vector<std::unique_ptr<AbstractResolver>> m_nonBlockingResolvers;
    CloudAddressResolver* m_cloudAddressResolver = nullptr;
    PredefinedHostResolver* m_predefinedHostResolver = nullptr;

private:
    static std::string_view toString(HostAddressInfo::State state);
};

} // namespace nx::network
