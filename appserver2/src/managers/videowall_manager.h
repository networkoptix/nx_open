#pragma once

#include <memory>
#include <transaction/transaction.h>
#include <nx_ec/managers/abstract_videowall_manager.h>
#include <core/resource_access/user_access_data.h>

namespace ec2 {

template<class QueryProcessorType>
class QnVideowallManager: public AbstractVideowallManager
{
public:
    QnVideowallManager(
        QueryProcessorType* const queryProcessor,
        const Qn::UserAccessData& userAccessData);

protected:
    virtual int getVideowalls(impl::GetVideowallsHandlerPtr handler) override;
    virtual int save(
        const nx::vms::api::VideowallData& resource,
        impl::SimpleHandlerPtr handler) override;
    virtual int remove(const QnUuid& id, impl::SimpleHandlerPtr handler) override;

    virtual int sendControlMessage(
        const nx::vms::api::VideowallControlMessageData& message,
        impl::SimpleHandlerPtr handler) override;

private:
    QueryProcessorType* const m_queryProcessor;
    Qn::UserAccessData m_userAccessData;
};

template<class QueryProcessorType>
QnVideowallManager<QueryProcessorType>::QnVideowallManager(
    QueryProcessorType* const queryProcessor,
    const Qn::UserAccessData& userAccessData)
    :
    m_queryProcessor(queryProcessor),
    m_userAccessData(userAccessData)
{
}

template<class QueryProcessorType>
int QnVideowallManager<QueryProcessorType>::getVideowalls(impl::GetVideowallsHandlerPtr handler)
{
    const int reqID = generateRequestID();
    auto queryDoneHandler = [reqID, handler](
        ErrorCode errorCode,
        const nx::vms::api::VideowallDataList& videowalls)
    {
        handler->done(reqID, errorCode, videowalls);
    };
    m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<QnUuid,
        nx::vms::api::VideowallDataList, decltype(queryDoneHandler)>(
        ApiCommand::getVideowalls,
        QnUuid(),
        queryDoneHandler);
    return reqID;
}

template<class T>
int QnVideowallManager<T>::save(
    const nx::vms::api::VideowallData& videowall,
    impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::saveVideowall,
        videowall,
        [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
    return reqID;
}

template<class T>
int QnVideowallManager<T>::remove(const QnUuid& id, impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::removeVideowall,
        nx::vms::api::IdData(id),
        [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
    return reqID;
}

template<class T>
int QnVideowallManager<T>::sendControlMessage(
    const nx::vms::api::VideowallControlMessageData& message,
    impl::SimpleHandlerPtr handler)
{
    const int reqID = generateRequestID();
    m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
        ApiCommand::videowallControl,
        message,
        [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
    return reqID;
}

} // namespace ec2
