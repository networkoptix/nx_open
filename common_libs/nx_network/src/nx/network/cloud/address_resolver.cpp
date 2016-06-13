#include "address_resolver.h"

#include "common/common_globals.h"

#include <nx/utils/std/future.h>
#include <nx/utils/log/log.h>
#include <nx/utils/thread/barrier_handler.h>
#include <nx/network/socket_global.h>
#include <nx/network/stun/cc/custom_stun.h>
#include <utils/serialization/lexical.h>

static const auto kDnsCacheTimeout = std::chrono::seconds(10);
static const auto kMediatorCacheTimeout = std::chrono::seconds(10);

namespace nx {
namespace network {
namespace cloud {

QString toString(const AddressType& type)
{
    switch(type)
    {
        case AddressType::unknown: return lit("unknown");
        case AddressType::direct: return lit("direct");
        case AddressType::cloud: return lit("cloud");
    };

    NX_ASSERT(false, Q_FUNC_INFO, "undefined AddressType");
    return lit("undefined=%1").arg(static_cast<int>(type));
}

TypedAddress::TypedAddress(HostAddress address_, AddressType type_)
:
    address(std::move(address_)),
    type(std::move(type_))
{
}

QString TypedAddress::toString() const
{
    return lm("%1(%2)").str(address).str(type);
}

AddressAttribute::AddressAttribute(AddressAttributeType type_, quint64 value_)
    : type(type_)
    , value(value_)
{
}

bool AddressAttribute::operator ==(const AddressAttribute& rhs) const
{
    return type == rhs.type && value == rhs.value;
}

bool AddressAttribute::operator <(const AddressAttribute& rhs) const
{
    return type < rhs.type && value < rhs.value;
}

QString AddressAttribute::toString() const
{
    switch(type)
    {
        case AddressAttributeType::unknown:
            return lit("unknown");
        case AddressAttributeType::port:
            return lit("port=%1").arg(value);
    };

    NX_ASSERT(false, Q_FUNC_INFO, "undefined AddressAttributeType");
    return lit("undefined=%1").arg(static_cast<int>(type));
}

AddressEntry::AddressEntry(AddressType type_, HostAddress host_)
    : type(type_)
    , host(std::move(host_))
{
}

AddressEntry::AddressEntry(const SocketAddress& address)
    : type(AddressType::direct)
    , host(address.address)
{
    attributes.push_back(AddressAttribute(
        AddressAttributeType::port, address.port));
}

bool AddressEntry::operator ==(const AddressEntry& rhs) const
{
    return type == rhs.type && host == rhs.host && attributes == rhs.attributes;
}

bool AddressEntry::operator <(const AddressEntry& rhs) const
{
    return type < rhs.type && host < rhs.host && attributes < rhs.attributes;
}

QString AddressEntry::toString() const
{
    return lm("%1:%2(%3)").str(type).str(host).container(attributes);
}

const QRegExp AddressResolver::kCloudAddressRegExp(QLatin1String(
    "(.+\\.)?[0-9a-f]{8}\\-[0-9a-f]{4}\\-[0-9a-f]{4}\\-[0-9a-f]{4}\\-[0-9a-f]{12}"));

AddressResolver::AddressResolver(
    std::shared_ptr<hpm::api::MediatorClientTcpConnection> mediatorConnection)
:
    m_mediatorConnection(std::move(mediatorConnection))
{
}

void AddressResolver::addFixedAddress(
    const HostAddress& hostName, const SocketAddress& hostAddress)
{
    NX_ASSERT(!hostName.isResolved(), Q_FUNC_INFO, "Hostname should be unresolved");
    NX_ASSERT(hostAddress.address.isResolved());

    NX_LOGX(lm("Added fixed address for %1: %2")
        .str(hostName).str(hostAddress), cl_logDEBUG2);

    QnMutexLocker lk(&m_mutex);
    AddressEntry entry(hostAddress);
    auto& entries = m_info.emplace(hostName, HostAddressInfo(hostName))
        .first->second.fixedEntries;

    const auto it = std::find(entries.begin(), entries.end(), entry);
    if (it == entries.end())
    {
        NX_LOGX(lm("New fixed address for %1: %2")
            .str(hostName).str(hostAddress), cl_logDEBUG1);

        entries.push_back(std::move(entry));

        // we possibly could also grabHandlers() to bring the result before DNS
        // is resolved (if in progress), but it does not cause a serious delay
        // so far
    }
}

void AddressResolver::removeFixedAddress(
    const HostAddress& hostName, const SocketAddress& hostAddress)
{

    NX_LOGX(lm("Removed fixed address for %1: %2")
        .str(hostName).str(hostAddress), cl_logDEBUG2);

    QnMutexLocker lk(&m_mutex);
    AddressEntry entry(hostAddress);
    const auto record = m_info.find(hostName);
    if (record == m_info.end())
        return;

    auto& entries = record->second.fixedEntries;
    const auto it = std::find(entries.begin(), entries.end(), entry);
    if (it != entries.end())
    {
        NX_LOGX(lm("Removed fixed address for %1: %2")
            .str(hostName.toString()).str(hostAddress.toString()), cl_logDEBUG1);

        entries.erase(it);
    }
}

void AddressResolver::resolveDomain(
    const HostAddress& domain,
    utils::MoveOnlyFunc<void(std::vector<TypedAddress>)> handler)
{
    m_mediatorConnection->resolveDomain(
        nx::hpm::api::ResolveDomainRequest(domain.toString().toUtf8()),
        [this, domain, handler = std::move(handler)](
            nx::hpm::api::ResultCode resultCode,
            nx::hpm::api::ResolveDomainResponse response)
        {
            NX_LOGX(lm("Domain %1 resolution on mediator result: %2")
                .arg(domain.toString()).arg(QnLexical::serialized(resultCode)),
                cl_logDEBUG2);

            std::vector<TypedAddress> result;
            {
                QnMutexLocker lk(&m_mutex);
                iterateSubdomains(
                    domain.toString(), [&](HaInfoIterator info)
                    {
                        result.emplace_back(info->first, AddressType::direct);
                        return false; // continue
                    });
            }

            for (const auto& host : response.hostNames)
            {
                HostAddress address(QString::fromUtf8(host));
                result.emplace_back(std::move(address), AddressType::cloud);
            }

            NX_LOGX(lm("Domain %1 is resolved to: %2")
                .arg(domain.toString()).container(result), cl_logDEBUG1);

            handler(std::move(result));
        });
}

void AddressResolver::resolveAsync(
    const HostAddress& hostName, ResolveHandler handler,
    bool natTraversal, void* requestId)
{
    if (hostName.isResolved())
    {
        AddressEntry entry(AddressType::direct, hostName);
        return handler(SystemError::noError, {std::move(entry)});
    }

    QnMutexLocker lk(&m_mutex);
    auto info = m_info.emplace(hostName, HostAddressInfo(hostName)).first;
    info->second.checkExpirations();
    tryFastDomainResolve(info);
    if (info->second.isResolved(natTraversal))
    {
        auto entries = info->second.getAll();
        lk.unlock();
        NX_LOGX(lm("Address %1 resolved from cache: %2")
            .str(hostName).container(entries), cl_logDEBUG2);

        return handler(SystemError::noError, std::move(entries));
    }

    info->second.pendingRequests.insert(requestId);
    m_requests.insert(std::make_pair(requestId,
        RequestInfo(info->first, natTraversal, std::move(handler))));

    NX_LOGX(lm("Address %1 will be resolved later by request %2")
        .str(hostName).arg(requestId), cl_logDEBUG1);

    if (info->second.isLikelyCloudAddress && natTraversal)
        mediatorResolve(info, &lk, true);
    else
        dnsResolve(info, &lk, natTraversal);
}

std::vector<AddressEntry> AddressResolver::resolveSync(
     const HostAddress& hostName, bool natTraversal = false)
{
    utils::promise<std::vector<AddressEntry>> promise;
    auto handler = [&](
        SystemError::ErrorCode code, std::vector<AddressEntry> entries)
    {
        if (code != SystemError::noError)
        {
            NX_LOGX(lm("Address %1 could not be resolved: %2")
                .str(hostName).arg(SystemError::toString(code)), cl_logERROR);
        }

        promise.set_value(std::move(entries));
    };

    resolveAsync(hostName, std::move(handler), natTraversal);
    return promise.get_future().get();
}

void AddressResolver::cancel(
    void* requestId, nx::utils::MoveOnlyFunc<void()> handler)
{
    boost::optional<utils::promise<bool>> promise;
    if (!handler)
    {
        // no handler means we have to wait for complete
        promise = utils::promise<bool>();
        handler = [&](){ promise->set_value(true); };
    }

    {
        BarrierHandler barrier(std::move(handler));
        QnMutexLocker lk(&m_mutex);
        const auto range = m_requests.equal_range(requestId);
        for (auto it = range.first; it != range.second;)
        {
            NX_ASSERT(!it->second.guard, Q_FUNC_INFO,
                "Cancel has already been called for this requestId");

            if (it->second.inProgress && !it->second.guard)
                (it++)->second.guard = barrier.fork();
            else
                it = m_requests.erase(it);
        }
    }

    if (promise)
        promise->get_future().wait();
}

bool AddressResolver::isRequestIdKnown(void* requestId) const
{
    QnMutexLocker lk(&m_mutex);
    return m_requests.count(requestId);
}

void AddressResolver::pleaseStop(nx::utils::MoveOnlyFunc<void()> handler)
{
    // TODO: make DnsResolver QnStoppableAsync
    m_dnsResolver.stop();
    m_mediatorConnection->pleaseStop(std::move(handler));
}

AddressResolver::HostAddressInfo::HostAddressInfo(const HostAddress& hostAddress)
:
    isLikelyCloudAddress(kCloudAddressRegExp.exactMatch(hostAddress.toString())),
    m_dnsState(State::unresolved),
    m_mediatorState(State::unresolved)
{
}

void AddressResolver::HostAddressInfo::setDnsEntries(
    std::vector<AddressEntry> entries)
{
    m_dnsState = State::resolved;
    m_dnsResolveTime = std::chrono::system_clock::now();
    m_dnsEntries = std::move(entries);
}

void AddressResolver::HostAddressInfo::setMediatorEntries(
        std::vector<AddressEntry> entries)
{
    m_mediatorState = State::resolved;
    m_mediatorResolveTime = std::chrono::system_clock::now();
    m_mediatorEntries = std::move(entries);
}

void AddressResolver::HostAddressInfo::checkExpirations()
{
    if (m_dnsState == State::resolved &&
        m_dnsResolveTime + kDnsCacheTimeout < std::chrono::system_clock::now())
    {
        m_dnsState = State::unresolved;
        m_dnsEntries.clear();
    }

    if (m_mediatorState == State::resolved &&
        m_mediatorResolveTime + kMediatorCacheTimeout < std::chrono::system_clock::now())
    {
        m_mediatorState = State::unresolved;
        m_mediatorEntries.clear();
    }
}

bool AddressResolver::HostAddressInfo::isResolved(bool natTraversal) const
{
    if(!fixedEntries.empty() || !m_dnsEntries.empty() || !m_mediatorEntries.empty())
        return true; // any address is better than nothing

    return (m_dnsState == State::resolved) &&
        (!natTraversal || m_mediatorState == State::resolved);
}

template<typename Container>
void containerAppend(Container& c1, const Container& c2)
{
    c1.insert(c1.end(), c2.begin(), c2.end());
}

std::vector<AddressEntry> AddressResolver::HostAddressInfo::getAll() const
{
    std::vector<AddressEntry> entries(fixedEntries);
    if (isLikelyCloudAddress)
    {
        containerAppend(entries, m_mediatorEntries);
        containerAppend(entries, m_dnsEntries);
    }
    else
    {
        containerAppend(entries, m_dnsEntries);
        containerAppend(entries, m_mediatorEntries);
    }

    return entries;
}

AddressResolver::RequestInfo::RequestInfo(
    HostAddress _address, bool _natTraversal, ResolveHandler _handler)
:
    address(std::move(_address)),
    inProgress(false),
    natTraversal(_natTraversal),
    handler(std::move(_handler))
{
}

void AddressResolver::tryFastDomainResolve(HaInfoIterator info)
{
    const auto domain = info->first.toString();
    if (domain.indexOf(lit(".")) != -1)
        return; // only top level domains might be fast resolved

    const bool hasFound = iterateSubdomains(
        domain, [&](HaInfoIterator other)
        {
            info->second.fixedEntries = other->second.fixedEntries;
            return true; // just resolve to first avaliable
        });

    if (!hasFound)
        info->second.fixedEntries.clear();
}

void AddressResolver::dnsResolve(
    HaInfoIterator info, QnMutexLockerBase* lk, bool needMediator)
{
    switch (info->second.dnsState())
    {
        case HostAddressInfo::State::resolved:
            if (needMediator) mediatorResolve(info, lk, false);
            return;
        case HostAddressInfo::State::inProgress:
            return;
        case HostAddressInfo::State::unresolved:
            break; // continue
    }

    info->second.dnsProgress();
    lk->unlock();
    m_dnsResolver.resolveAddressAsync(
        info->first,
        [this, info, needMediator](
            SystemError::ErrorCode code, const HostAddress& host)
        {
            const HostAddress hostAddress(host.inAddr());
            std::vector<Guard> guards;

            QnMutexLocker lk(&m_mutex);
            std::vector<AddressEntry> entries;
            if(code == SystemError::noError)
                entries.push_back(AddressEntry(AddressType::direct, hostAddress));

            NX_LOGX(lm("Address %1 is resolved by DNS to %2")
                .str(info->first).container(entries), cl_logDEBUG2);

            info->second.setDnsEntries(std::move(entries));
            guards = grabHandlers(code, info);
            if (needMediator && !info->second.isResolved(true))
                mediatorResolve(info, &lk, false); // in case it's not resolved yet
        },
        this);
}

void AddressResolver::mediatorResolve(
    HaInfoIterator info, QnMutexLockerBase* lk, bool needDns)
{
    switch (info->second.mediatorState())
    {
        case HostAddressInfo::State::resolved:
            if (needDns) dnsResolve(info, lk, false);
            return;
        case HostAddressInfo::State::inProgress:
            return;
        case HostAddressInfo::State::unresolved:
            break; // continue
    }

    info->second.mediatorProgress();
    lk->unlock();
    m_mediatorConnection->resolvePeer(
        nx::hpm::api::ResolvePeerRequest(info->first.toString().toUtf8()),
        [this, info, needDns](
            nx::hpm::api::ResultCode resultCode,
            nx::hpm::api::ResolvePeerResponse response)
        {
            std::vector<Guard> guards;

            QnMutexLocker lk(&m_mutex);
            std::vector<AddressEntry> entries;
            if (resultCode == nx::hpm::api::ResultCode::ok)
            {
                for (const auto& it : response.endpoints)
                {
                    AddressEntry entry(AddressType::direct, it.address);
                    entry.attributes.push_back(AddressAttribute(
                        AddressAttributeType::port, it.port));
                    entries.push_back(std::move(entry));
                }

                // if target host supports cloud connect, adding corresponding
                // address entry
                if (response.connectionMethods != 0)
                    entries.emplace_back(AddressType::cloud, info->first);
            }

            NX_LOGX(lm("Address %1 is resolved by mediator to %2")
                .str(info->first).container(entries), cl_logDEBUG2);

            const auto code = (resultCode == nx::hpm::api::ResultCode::ok)
                ? SystemError::noError
                : SystemError::hostUnreach; //TODO #ak correct error translation

            info->second.setMediatorEntries(std::move(entries));
            guards = grabHandlers(code, info);
            if (needDns && !info->second.isResolved(true))
                dnsResolve(info, &lk, false); // in case it's not resolved yet
        });
}

std::vector<Guard> AddressResolver::grabHandlers(
        SystemError::ErrorCode lastErrorCode, HaInfoIterator info)
{
    std::vector<Guard> guards;

    auto entries = info->second.getAll();
    for (auto req = info->second.pendingRequests.begin();
         req != info->second.pendingRequests.end();)
    {
        const auto range = m_requests.equal_range(*req);
        bool noPending = (range.first == range.second);
        for (auto it = range.first; it != range.second; ++it)
        {
            if (it->second.address != info->first ||
                it->second.inProgress ||
                !info->second.isResolved(it->second.natTraversal))
            {
                noPending = false;
                continue;
            }

            it->second.inProgress = true;
            guards.push_back(Guard(
                [this, it, entries, lastErrorCode, info]() {
                    auto resolveDataPair = std::move(*it);
                    {
                        QnMutexLocker lk(&m_mutex);
                        m_requests.erase(it);
                    }

                    auto code = entries.empty() ? lastErrorCode : SystemError::noError;
                    resolveDataPair.second.handler(code, std::move(entries));

                    QnMutexLocker lk(&m_mutex);
                    NX_LOGX(lm("Address %1 is resolved by request %2 to %3")
                        .str(info->first).arg(resolveDataPair.first)
                        .container(entries), cl_logDEBUG2);
                    m_condition.wakeAll();
                }));
        }

        if(noPending)
            req = info->second.pendingRequests.erase(req);
        else
            ++req;
    }

    if (guards.size())
    {
        NX_LOGX(lm("There is(are) %1 about to be notified: %2 is resolved to %3")
            .arg(guards.size()).str(info->first).container(entries), cl_logDEBUG1);
    }

    return guards;
}

} // namespace cloud
} // namespace network
} // namespace nx
