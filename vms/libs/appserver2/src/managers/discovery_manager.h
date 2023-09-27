// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource_access/user_access_data.h>
#include <nx/vms/discovery/manager.h>
#include <nx_ec/managers/abstract_discovery_manager.h>
#include <transaction/transaction.h>

namespace ec2 {

// TODO: #vkutin #muskov Think where to put these globals.
nx::vms::api::DiscoveryData toApiDiscoveryData(
    const QnUuid &id, const nx::utils::Url &url, bool ignore);

template<class QueryProcessorType>
class QnDiscoveryManager: public AbstractDiscoveryManager
{
public:
    QnDiscoveryManager(QueryProcessorType* queryProcessor, const Qn::UserSession& userSession);

    virtual int discoverPeer(
        const QnUuid& id,
        const nx::utils::Url& url,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int addDiscoveryInformation(
        const QnUuid& id,
        const nx::utils::Url& url,
        bool ignore,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int removeDiscoveryInformation(
        const QnUuid& id,
        const nx::utils::Url& url,
        bool ignore,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int getDiscoveryData(
        Handler<nx::vms::api::DiscoveryDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

private:
    decltype(auto) processor() { return m_queryProcessor->getAccess(m_userSession); }

private:
    QueryProcessorType* const m_queryProcessor;
    Qn::UserSession m_userSession;
};

template<class QueryProcessorType>
QnDiscoveryManager<QueryProcessorType>::QnDiscoveryManager(
    QueryProcessorType* queryProcessor, const Qn::UserSession& userSession)
    :
    m_queryProcessor(queryProcessor),
    m_userSession(userSession)
{
}

template<class QueryProcessorType>
int QnDiscoveryManager<QueryProcessorType>::discoverPeer(
    const QnUuid& id,
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
    const QnUuid& id,
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
    const QnUuid& id,
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
    processor().template processQueryAsync<QnUuid, nx::vms::api::DiscoveryDataList>(
        ApiCommand::getDiscoveryData,
        QnUuid(),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

} // namespace ec2
