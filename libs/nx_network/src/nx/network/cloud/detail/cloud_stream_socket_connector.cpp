// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cloud_stream_socket_connector.h"

#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>

namespace nx::network::cloud::detail {

CloudStreamSocketConnector::CloudStreamSocketConnector(
    int ipVersion,
    const SocketAddress& addr)
    :
    m_ipVersion(ipVersion),
    m_addr(addr)
{
}

CloudStreamSocketConnector::~CloudStreamSocketConnector()
{
    if (isInSelfAioThread())
        pleaseStopSync();
}

void CloudStreamSocketConnector::connect(
    std::chrono::milliseconds timeout,
    StreamSocketAttributes attrs,
    CompletionHandler handler)
{
    NX_VERBOSE(this, "Connecting to %1 with timeout %2", m_addr, timeout);

    m_timeout = timeout;
    m_attrs = std::move(attrs);
    m_handler = std::move(handler);

    nx::network::SocketGlobals::instance().addressResolver().resolveAsync(
        m_addr.address,
        [this, operationGuard = m_guard.sharedGuard(), port = m_addr.port](
            SystemError::ErrorCode code, std::deque<AddressEntry> dnsEntries) mutable
        {
            if (auto lock = operationGuard->lock())
            {
                NX_VERBOSE(this, "Address resolved. Code: %1, entries: %2",
                    code, dnsEntries.size());

                post([this, port, code, dnsEntries = std::move(dnsEntries)]() mutable
                {
                    if (code != SystemError::noError)
                        return nx::utils::swapAndCall(m_handler, code, std::nullopt, nullptr);

                    if (dnsEntries.empty())
                    {
                        NX_ASSERT(false, "Non-zero error is expected instead of empty addr list");
                        return nx::utils::swapAndCall(m_handler,
                            SystemError::hostNotFound, std::nullopt, nullptr);
                    }

                    connectToEntriesAsync(std::move(dnsEntries), port);
                });
            }
        },
        NatTraversalSupport::enabled,
        m_ipVersion,
        this);
}

void CloudStreamSocketConnector::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_guard->terminate();
    nx::network::SocketGlobals::addressResolver().cancel(this);
}

void CloudStreamSocketConnector::connectToEntriesAsync(
    std::deque<AddressEntry> dnsEntries,
    int port)
{
    for (auto& entry: dnsEntries)
    {
        if (entry.type != AddressType::direct)
            continue;

        auto portAttrIter = std::find_if(
            entry.attributes.begin(), entry.attributes.end(),
            [](const AddressAttribute& attr) { return attr.type == AddressAttributeType::port; });
        if (portAttrIter == entry.attributes.end())
            entry.attributes.push_back(AddressAttribute(AddressAttributeType::port, port));
    }

    m_multipleAddressConnector = std::make_unique<AnyAccessibleAddressConnector>(
        m_ipVersion,
        std::move(dnsEntries));
    addDependant(m_multipleAddressConnector.get(), [this]() { m_multipleAddressConnector.reset(); });
    m_multipleAddressConnector->connectAsync(
        m_timeout,
        std::exchange(m_attrs, {}),
        [this](SystemError::ErrorCode err, auto&&... args)
        {
            NX_VERBOSE(this, "Connect to %1 completed with %2", m_addr, SystemError::toString(err));
            nx::utils::swapAndCall(m_handler, err, std::forward<decltype(args)>(args)...);
        });
}

} // namespace nx::network::cloud::detail
