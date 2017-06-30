#include "layout_tour_manager.h"

#include "fixed_url_client_query_processor.h"
#include "server_query_processor.h"

namespace ec2 {

void QnLayoutTourNotificationManager::triggerNotification(const QnTransaction<ApiIdData>& tran,
    NotificationSource /*source*/)
{
    NX_ASSERT(tran.command == ApiCommand::removeLayoutTour);
    emit removed(QnUuid(tran.params.id));
}

void QnLayoutTourNotificationManager::triggerNotification(
    const QnTransaction<ApiLayoutTourData>& tran,
    NotificationSource source)
{
    NX_ASSERT(tran.command == ApiCommand::saveLayoutTour);
    emit addedOrUpdated(tran.params, source);
}

template<typename QueryProcessorType>
QnLayoutTourManager<QueryProcessorType>::QnLayoutTourManager(
    QueryProcessorType* const queryProcessor,
    const Qn::UserAccessData &userAccessData)
    :
    m_queryProcessor(queryProcessor),
    m_userAccessData(userAccessData)
{
}

template<class QueryProcessorType>
int QnLayoutTourManager<QueryProcessorType>::getLayoutTours(impl::GetLayoutToursHandlerPtr handler)
{
    const int reqID = generateRequestID();
    auto queryDoneHandler =
        [reqID, handler](ErrorCode errorCode, const ApiLayoutTourDataList& tours)
        {
            handler->done(reqID, errorCode, tours);
        };
    m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync
        <std::nullptr_t, ApiLayoutTourDataList, decltype(queryDoneHandler)>
        (ApiCommand::getLayoutTours, nullptr, queryDoneHandler);
    return reqID;
}

template<class QueryProcessorType>
int QnLayoutTourManager<QueryProcessorType>::save(const ec2::ApiLayoutTourData& tour, impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
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
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::removeLayoutTour, ApiIdData(id),
        [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
    return reqID;
}

template class QnLayoutTourManager<FixedUrlClientQueryProcessor>;
template class QnLayoutTourManager<ServerQueryProcessorAccess>;

} // namespace ec2
