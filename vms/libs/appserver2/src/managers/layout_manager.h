#pragma once

#include <transaction/transaction.h>

#include <nx/vms/api/data/layout_data.h>
#include <nx_ec/managers/abstract_layout_manager.h>
#include <core/resource_access/user_access_data.h>

namespace ec2 {
template<class QueryProcessorType>
class QnLayoutManager: public AbstractLayoutManager
{
public:
    QnLayoutManager(
        QueryProcessorType* const queryProcessor,
        const Qn::UserAccessData& userAccessData);

    virtual int getLayouts(impl::GetLayoutsHandlerPtr handler) override;
    virtual int save(
        const nx::vms::api::LayoutData& layout,
        impl::SimpleHandlerPtr handler) override;
    virtual int remove(const QnUuid& resource, impl::SimpleHandlerPtr handler) override;

private:
    QueryProcessorType* const m_queryProcessor;
    Qn::UserAccessData m_userAccessData;
};

template<typename QueryProcessorType>
QnLayoutManager<QueryProcessorType>::QnLayoutManager(
    QueryProcessorType* const queryProcessor,
    const Qn::UserAccessData& userAccessData)
    : m_queryProcessor(queryProcessor),
    m_userAccessData(userAccessData)
{
}

template<class QueryProcessorType>
int QnLayoutManager<QueryProcessorType>::getLayouts(impl::GetLayoutsHandlerPtr handler)
{
    const int reqID = generateRequestID();
    auto queryDoneHandler =
        [reqID, handler](
        ErrorCode errorCode,
        const nx::vms::api::LayoutDataList& layouts)
        {
            handler->done(reqID, errorCode, layouts);
        };
    m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<const QnUuid&,
        nx::vms::api::LayoutDataList, decltype(queryDoneHandler)>(
        ApiCommand::getLayouts,
        QnUuid(),
        queryDoneHandler);
    return reqID;
}

template<class QueryProcessorType>
int QnLayoutManager<QueryProcessorType>::save(
    const nx::vms::api::LayoutData& layout,
    impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::saveLayout,
        layout,
        [handler, reqID](ec2::ErrorCode errorCode)
            {
                handler->done(reqID, errorCode);
            });
    return reqID;
}

template<class QueryProcessorType>
int QnLayoutManager<QueryProcessorType>::remove(const QnUuid& id, impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::removeLayout,
        nx::vms::api::IdData(id),
        [handler, reqID](ec2::ErrorCode errorCode)
            {
                handler->done(reqID, errorCode);
            });
    return reqID;
}

} // namespace ec2
