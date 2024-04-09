// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/rest/user_access_data.h>
#include <nx/vms/discovery/manager.h>
#include <nx_ec/managers/abstract_discovery_manager.h>
#include <transaction/transaction.h>

namespace ec2 {

// TODO: #vkutin #muskov Think where to put these globals.
nx::vms::api::DiscoveryData toApiDiscoveryData(
    const nx::Uuid &id, const nx::utils::Url &url, bool ignore);

template<class QueryProcessorType>
class QnDiscoveryManager: public AbstractDiscoveryManager
{
public:
    QnDiscoveryManager(QueryProcessorType* queryProcessor, const nx::network::rest::audit::Record& auditRecord);

    virtual int discoverPeer(
        const nx::Uuid& id,
        const nx::utils::Url& url,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int addDiscoveryInformation(
        const nx::Uuid& id,
        const nx::utils::Url& url,
        bool ignore,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int removeDiscoveryInformation(
        const nx::Uuid& id,
        const nx::utils::Url& url,
        bool ignore,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int getDiscoveryData(
        Handler<nx::vms::api::DiscoveryDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

private:
    decltype(auto) processor() { return m_queryProcessor->getAccess(m_auditRecord); }

private:
    QueryProcessorType* const m_queryProcessor;
    nx::network::rest::audit::Record m_auditRecord;
};

template<class QueryProcessorType>
QnDiscoveryManager<QueryProcessorType>::QnDiscoveryManager(
    QueryProcessorType* queryProcessor, const nx::network::rest::audit::Record& auditRecord)
    :
    m_queryProcessor(queryProcessor),
    m_auditRecord(auditRecord)
{
}

template<class QueryProcessorType>
int QnDiscoveryManager<QueryProcessorType>::discoverPeer(
    const nx::Uuid& id,
    const nx::utils::Url& url,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    nx::vms::api::DiscoverPeerData params;
    params.id = id;
    params.url = url.toString();

    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::discoverPeer,
        params,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnDiscoveryManager<QueryProcessorType>::addDiscoveryInformation(
    const nx::Uuid& id,
    const nx::utils::Url& url,
    bool ignore,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    NX_ASSERT(!url.host().isEmpty());
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::addDiscoveryInformation,
        toApiDiscoveryData(id, url, ignore),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnDiscoveryManager<QueryProcessorType>::removeDiscoveryInformation(
    const nx::Uuid& id,
    const nx::utils::Url& url,
    bool ignore,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeDiscoveryInformation,
        toApiDiscoveryData(id, url, ignore),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class QueryProcessorType>
int QnDiscoveryManager<QueryProcessorType>::getDiscoveryData(
    Handler<nx::vms::api::DiscoveryDataList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().template processQueryAsync<nx::Uuid, nx::vms::api::DiscoveryDataList>(
        ApiCommand::getDiscoveryData,
        nx::Uuid(),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

} // namespace ec2
