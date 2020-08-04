#pragma once

#include <transaction/transaction.h>

#include <nx/vms/api/data/layout_tour_data.h>
#include <nx_ec/managers/abstract_layout_tour_manager.h>
#include <core/resource_access/user_access_data.h>

namespace ec2 {

template<class QueryProcessorType>
class QnLayoutTourManager: public AbstractLayoutTourManager
{
public:
    QnLayoutTourManager(QueryProcessorType* const queryProcessor, const Qn::UserSession& userSession);

    virtual int getLayoutTours(impl::GetLayoutToursHandlerPtr handler) override;
    virtual int save(
        const nx::vms::api::LayoutTourData& tour,
        impl::SimpleHandlerPtr handler) override;
    virtual int remove(const QnUuid& tour, impl::SimpleHandlerPtr handler) override;

private:
    QueryProcessorType* const m_queryProcessor;
    Qn::UserSession m_userSession;
};

template<typename QueryProcessorType>
QnLayoutTourManager<QueryProcessorType>::QnLayoutTourManager(
    QueryProcessorType* const queryProcessor,
    const Qn::UserSession& userSession)
    :
    m_queryProcessor(queryProcessor),
    m_userSession(userSession)
{
}

template<class QueryProcessorType>
int QnLayoutTourManager<QueryProcessorType>::getLayoutTours(impl::GetLayoutToursHandlerPtr handler)
{
    const int reqID = generateRequestID();
    auto queryDoneHandler =
        [reqID, handler](ErrorCode errorCode, const nx::vms::api::LayoutTourDataList& tours)
        {
            handler->done(reqID, errorCode, tours);
        };
    m_queryProcessor->getAccess(m_userSession).template processQueryAsync
        <const QnUuid&, nx::vms::api::LayoutTourDataList, decltype(queryDoneHandler)>
        (ApiCommand::getLayoutTours, QnUuid(), queryDoneHandler);
    return reqID;
}

template<class QueryProcessorType>
int QnLayoutTourManager<QueryProcessorType>::save(
    const nx::vms::api::LayoutTourData& tour,
    impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userSession).processUpdateAsync(
        ApiCommand::saveLayoutTour,
        tour,
        [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
    return reqID;
}

template<class QueryProcessorType>
int QnLayoutTourManager<QueryProcessorType>::remove(const QnUuid& id, impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userSession).processUpdateAsync(
        ApiCommand::removeLayoutTour, nx::vms::api::IdData(id),
        [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
    return reqID;
}

} // namespace ec2
