#pragma once

#include <transaction/transaction.h>

#include <nx_ec/data/api_layout_tour_data.h>
#include <nx_ec/managers/abstract_layout_tour_manager.h>
#include <core/resource_access/user_access_data.h>

namespace ec2 {

class QnLayoutTourNotificationManager: public AbstractLayoutTourNotificationManager
{
public:
    void triggerNotification(const QnTransaction<ApiIdData>& tran, NotificationSource source);
    void triggerNotification(const QnTransaction<ApiLayoutTourData>& tran, NotificationSource source);
};

typedef std::shared_ptr<QnLayoutTourNotificationManager> QnLayoutTourNotificationManagerPtr;

template<class QueryProcessorType>
class QnLayoutTourManager: public AbstractLayoutTourManager
{
public:
    QnLayoutTourManager(QueryProcessorType* const queryProcessor, const Qn::UserAccessData &userAccessData);

    virtual int getLayoutTours(impl::GetLayoutToursHandlerPtr handler) override;
    virtual int save(const ec2::ApiLayoutTourData& tour, impl::SimpleHandlerPtr handler) override;
    virtual int remove(const QnUuid& tour, impl::SimpleHandlerPtr handler) override;

private:
    QueryProcessorType* const m_queryProcessor;
    Qn::UserAccessData m_userAccessData;
};

} // namespace ec2
