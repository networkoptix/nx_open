#pragma once

#include <transaction/transaction.h>
#include <nx_ec/managers/abstract_webpage_manager.h>
#include <core/resource_access/user_access_data.h>

namespace ec2 {

template<class QueryProcessorType>
class QnWebPageManager: public AbstractWebPageManager
{
public:
    QnWebPageManager(
        QueryProcessorType* const queryProcessor,
        const Qn::UserSession& userSession);

protected:
    virtual int getWebPages(impl::GetWebPagesHandlerPtr handler) override;
    virtual int save(
        const nx::vms::api::WebPageData& webpage,
        impl::SimpleHandlerPtr handler) override;
    virtual int remove(const QnUuid& id, impl::SimpleHandlerPtr handler) override;

private:
    QueryProcessorType* const m_queryProcessor;
    Qn::UserSession m_userSession;
};

template<class QueryProcessorType>
QnWebPageManager<QueryProcessorType>::QnWebPageManager(
    QueryProcessorType* const queryProcessor,
    const Qn::UserSession& userSession)
    :
    m_queryProcessor(queryProcessor),
    m_userSession(userSession)
{
}

template<class QueryProcessorType>
int QnWebPageManager<QueryProcessorType>::getWebPages(impl::GetWebPagesHandlerPtr handler)
{
    const int reqID = generateRequestID();
    auto queryDoneHandler = [reqID, handler](
        ErrorCode errorCode,
        const nx::vms::api::WebPageDataList& webpages)
        {
            handler->done(reqID, errorCode, webpages);
        };
    m_queryProcessor->getAccess(m_userSession).template processQueryAsync<QnUuid,
        nx::vms::api::WebPageDataList, decltype(queryDoneHandler)>(
        ApiCommand::getWebPages,
        QnUuid(),
        queryDoneHandler);
    return reqID;
}

template<class QueryProcessorType>
int QnWebPageManager<QueryProcessorType>::save(
    const nx::vms::api::WebPageData& webpage,
    impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userSession).processUpdateAsync(
        ApiCommand::saveWebPage,
        webpage,
        [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
    return reqID;
}

template<class QueryProcessorType>
int QnWebPageManager<QueryProcessorType>::remove(const QnUuid& id, impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userSession).processUpdateAsync(
        ApiCommand::removeWebPage,
        nx::vms::api::IdData(id),
        [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
    return reqID;
}

} // namespace ec2
