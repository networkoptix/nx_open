// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource_access/user_access_data.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/log/log.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <nx_ec/ec_api_common.h>
#include <nx_ec/managers/abstract_resource_manager.h>
#include <transaction/transaction.h>

namespace ec2 {

template<class QueryProcessorType>
class QnResourceManager: public AbstractResourceManager
{
public:
    QnResourceManager(QueryProcessorType* queryProcessor, const Qn::UserSession& userSession);

    virtual int getResourceTypes(
        Handler<QnResourceTypeList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int setResourceStatus(
        const QnUuid& resourceId,
        nx::vms::api::ResourceStatus status,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int getKvPairs(
        const QnUuid& resourceId,
        Handler<nx::vms::api::ResourceParamWithRefDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int getStatusList(
        const QnUuid& resourceId,
        Handler<nx::vms::api::ResourceStatusDataList> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int save(
        const nx::vms::api::ResourceParamWithRefDataList& dataList,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int remove(
        const QnUuid& resourceId,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int remove(
        const QVector<QnUuid>& resourceIds,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    virtual int remove(
        const nx::vms::api::ResourceParamWithRefData& data,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

    // TODO: #vbreus Temporary implementation, remove as REST API request will be implemented.
    virtual int removeHardwareIdMapping(
        const QnUuid& resourceId,
        Handler<> handler,
        nx::utils::AsyncHandlerExecutor handlerExecutor = {}) override;

private:
    decltype(auto) processor() { return m_queryProcessor->getAccess(m_userSession); }

private:
    QueryProcessorType* const m_queryProcessor;
    Qn::UserSession m_userSession;
};

template<class T>
QnResourceManager<T>::QnResourceManager(
    T* queryProcessor, const Qn::UserSession& userSession)
    :
    m_queryProcessor(queryProcessor),
    m_userSession(userSession)
{
}

template<class T>
int QnResourceManager<T>::getResourceTypes(
    Handler<QnResourceTypeList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().template processQueryAsync<std::nullptr_t, nx::vms::api::ResourceTypeDataList>(
        ApiCommand::getResourceTypes,
        nullptr,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](
            Result result, const nx::vms::api::ResourceTypeDataList& resTypeList) mutable
        {
            QnResourceTypeList outResTypeList;
            if (result)
                fromApiToResourceList(resTypeList, outResTypeList);
            handler(requestId, std::move(result), std::move(outResTypeList));
        });
    return requestId;
}

template<class T>
int QnResourceManager<T>::setResourceStatus(
    const QnUuid& resourceId,
    nx::vms::api::ResourceStatus status,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    nx::vms::api::ResourceStatusData params;
    params.id = resourceId;
    params.status = static_cast<nx::vms::api::ResourceStatus>(status);

    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::setResourceStatus,
        params,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](Result result) mutable
        {
            handler(requestId, std::move(result));
        });
    return requestId;
}

template<class T>
int QnResourceManager<T>::getKvPairs(
    const QnUuid& resourceId,
    Handler<nx::vms::api::ResourceParamWithRefDataList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().template processQueryAsync<QnUuid, nx::vms::api::ResourceParamWithRefDataList>(
        ApiCommand::getResourceParams,
        resourceId,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int QnResourceManager<T>::getStatusList(
    const QnUuid& resourceId,
    Handler<nx::vms::api::ResourceStatusDataList> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().template processQueryAsync<QnUuid, nx::vms::api::ResourceStatusDataList>(
        ApiCommand::getStatusList,
        resourceId,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int QnResourceManager<T>::save(
    const nx::vms::api::ResourceParamWithRefDataList& dataList,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::setResourceParams,
        dataList,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int QnResourceManager<T>::remove(
    const QnUuid& resourceId,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeResource,
        nx::vms::api::IdData(resourceId),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int QnResourceManager<T>::remove(
    const nx::vms::api::ResourceParamWithRefData& data,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeResourceParam,
        data,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int QnResourceManager<T>::remove(
    const QVector<QnUuid>& resourceIds,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    nx::vms::api::IdDataList params;
    for (const QnUuid& id: resourceIds)
        params.push_back(id);

    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeResources,
        params,
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

template<class T>
int QnResourceManager<T>::removeHardwareIdMapping(
    const QnUuid& resourceId,
    Handler<> handler,
    nx::utils::AsyncHandlerExecutor handlerExecutor)
{
    const int requestId = generateRequestID();
    processor().processUpdateAsync(
        ApiCommand::removeHardwareIdMapping,
        nx::vms::api::IdData(resourceId),
        [requestId, handler = handlerExecutor.bind(std::move(handler))](auto&&... args) mutable
        {
            handler(requestId, std::move(args)...);
        });
    return requestId;
}

} // namespace ec2
