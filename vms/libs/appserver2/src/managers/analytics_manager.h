#pragma once

#include <transaction/transaction.h>
#include <core/resource_access/user_access_data.h>
#include <nx_ec/managers/abstract_analytics_manager.h>

namespace ec2 {

template<typename QueryProcessor>
class AnalyticsManager: public AbstractAnalyticsManager
{

public:
    AnalyticsManager(
        QueryProcessor* queryProcessor,
        const Qn::UserAccessData& userAccessData);

protected:
    virtual int getAnalyticsPlugins(impl::GetAnalyticsPluginsHandlerPtr handler) override;

    virtual int getAnalyticsEngines(
        impl::GetAnalyticsEnginesHandlerPtr handler) override;

    virtual int save(
        const nx::vms::api::AnalyticsPluginData& analyticsPlugin,
        impl::SimpleHandlerPtr handler) override;

    virtual int save(
        const nx::vms::api::AnalyticsEngineData& analyticsEngine,
        impl::SimpleHandlerPtr handler) override;

    virtual int removeAnalyticsPlugin(const QnUuid& id, impl::SimpleHandlerPtr handler) override;

    virtual int removeAnalyticsEngine(
        const QnUuid& id,
        impl::SimpleHandlerPtr handler) override;

private:
    QueryProcessor* m_queryProcessor;
    Qn::UserAccessData m_userAccessData;

};

template<typename QueryProcessor>
AnalyticsManager<QueryProcessor>::AnalyticsManager(
    QueryProcessor* queryProcessor,
    const Qn::UserAccessData& userAccessData)
    :
    m_queryProcessor(queryProcessor),
    m_userAccessData(userAccessData)
{
}

template<typename QueryProcessor>
int AnalyticsManager<QueryProcessor>::getAnalyticsPlugins(
    impl::GetAnalyticsPluginsHandlerPtr handler)
{
    const int requestId = generateRequestID();
    auto queryDoneHandler =
        [requestId, handler](
            ErrorCode errorCode,
            const nx::vms::api::AnalyticsPluginDataList& analyticsPlugins)
        {
            handler->done(requestId, errorCode, analyticsPlugins);
        };

    m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<QnUuid,
        nx::vms::api::AnalyticsPluginDataList, decltype(queryDoneHandler)>(
            ApiCommand::getAnalyticsPlugins,
            QnUuid(),
            queryDoneHandler);

    return requestId;
}

template<typename QueryProcessor>
int AnalyticsManager<QueryProcessor>::getAnalyticsEngines(
    impl::GetAnalyticsEnginesHandlerPtr handler)
{
    const int requestId = generateRequestID();
    auto queryDoneHandler =
        [requestId, handler](
            ErrorCode errorCode,
            const nx::vms::api::AnalyticsEngineDataList& analyticsEngines)
        {
            handler->done(requestId, errorCode, analyticsEngines);
        };

    m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<QnUuid,
        nx::vms::api::AnalyticsEngineDataList, decltype(queryDoneHandler)>(
            ApiCommand::getAnalyticsEngines,
            QnUuid(),
            queryDoneHandler);

    return requestId;
}

template<typename QueryProcessor>
int AnalyticsManager<QueryProcessor>::save(
    const nx::vms::api::AnalyticsPluginData& analyticsPlugin,
    impl::SimpleHandlerPtr handler)
{
    const int requestId = generateRequestID();
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::saveAnalyticsPlugin,
        analyticsPlugin,
        [handler, requestId](ec2::ErrorCode errorCode)
        {
            handler->done(requestId, errorCode);
        });
    return requestId;
}

template<typename QueryProcessor>
int AnalyticsManager<QueryProcessor>::save(
    const nx::vms::api::AnalyticsEngineData& analyticsEngine,
    impl::SimpleHandlerPtr handler)
{
    const int requestId = generateRequestID();
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::saveAnalyticsEngine,
        analyticsEngine,
        [handler, requestId](ec2::ErrorCode errorCode)
        {
            handler->done(requestId, errorCode);
        });
    return requestId;
}

template<typename QueryProcessor>
int AnalyticsManager<QueryProcessor>::removeAnalyticsPlugin(
    const QnUuid& id,
    impl::SimpleHandlerPtr handler)
{
    const int requestId = generateRequestID();
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::removeAnalyticsPlugin,
        nx::vms::api::IdData(id),
        [handler, requestId](ec2::ErrorCode errorCode)
        {
            handler->done(requestId, errorCode);
        });
    return requestId;
}

template<typename QueryProcessor>
int AnalyticsManager<QueryProcessor>::removeAnalyticsEngine(
    const QnUuid& id,
    impl::SimpleHandlerPtr handler)
{
    const int requestId = generateRequestID();
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::removeAnalyticsEngine,
        nx::vms::api::IdData(id),
        [handler, requestId](ec2::ErrorCode errorCode)
        {
            handler->done(requestId, errorCode);
        });
    return requestId;
}

} // namespace ec2
