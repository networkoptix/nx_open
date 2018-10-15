#pragma once

#include <transaction/transaction.h>
#include <core/resource_access/user_access_data.h>
#include <nx_ec/managers/abstract_analytics_manager.h>

namespace ec2 {

template<typename QueryProcessorType>
class AnalyticsManager: public AbstractAnalyticsManager
{

public:
    AnalyticsManager(
        QueryProcessorType* const queryProcessor,
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
    QueryProcessorType* const m_queryProcessor;
    Qn::UserAccessData m_userAccessData;

};

template<typename QueryProcessorType>
AnalyticsManager<QueryProcessorType>::AnalyticsManager(
    QueryProcessorType* const queryProcessor,
    const Qn::UserAccessData& userAccessData)
    :
    m_queryProcessor(queryProcessor),
    m_userAccessData(userAccessData)
{
}


template<typename QueryProcessorType>
int AnalyticsManager<QueryProcessorType>::getAnalyticsPlugins(
    impl::GetAnalyticsPluginsHandlerPtr handler)
{
    const int reqID = generateRequestID();
    auto queryDoneHandler =
        [reqID, handler](
            ErrorCode errorCode,
            const nx::vms::api::AnalyticsPluginDataList& analyticsPlugins)
        {
            handler->done(reqID, errorCode, analyticsPlugins);
        };

    m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<QnUuid,
        nx::vms::api::AnalyticsPluginDataList, decltype(queryDoneHandler)>(
            ApiCommand::getAnalyticsPlugins,
            QnUuid(),
            queryDoneHandler);

    return reqID;
}

template<typename QueryProcessorType>
int AnalyticsManager<QueryProcessorType>::getAnalyticsEngines(
    impl::GetAnalyticsEnginesHandlerPtr handler)
{
    const int reqID = generateRequestID();
    auto queryDoneHandler =
        [reqID, handler](
            ErrorCode errorCode,
            const nx::vms::api::AnalyticsEngineDataList& analyticsEngines)
        {
            handler->done(reqID, errorCode, analyticsEngines);
        };

    m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<QnUuid,
        nx::vms::api::AnalyticsEngineDataList, decltype(queryDoneHandler)>(
            ApiCommand::getAnalyticsEngines,
            QnUuid(),
            queryDoneHandler);

    return reqID;
}

template<typename QueryProcessorType>
int AnalyticsManager<QueryProcessorType>::save(
    const nx::vms::api::AnalyticsPluginData& analyticsPlugin,
    impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::saveAnalyticsPlugin,
        analyticsPlugin,
        [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
    return reqID;
}

template<typename QueryProcessorType>
int AnalyticsManager<QueryProcessorType>::save(
    const nx::vms::api::AnalyticsEngineData& analyticsEngine,
    impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::saveAnalyticsEngine,
        analyticsEngine,
        [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
    return reqID;
}

template<typename QueryProcessorType>
int AnalyticsManager<QueryProcessorType>::removeAnalyticsPlugin(
    const QnUuid& id,
    impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::removeAnalyticsPlugin,
        nx::vms::api::IdData(id),
        [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
    return reqID;
}

template<typename QueryProcessorType>
int AnalyticsManager<QueryProcessorType>::removeAnalyticsEngine(
    const QnUuid& id,
    impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::removeAnalyticsEngine,
        nx::vms::api::IdData(id),
        [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
    return reqID;
}

} // namespace ec2
