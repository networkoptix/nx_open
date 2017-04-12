#pragma once

#include <transaction/transaction.h>

#include <nx_ec/data/api_layout_data.h>
#include <nx_ec/managers/abstract_layout_manager.h>
#include <core/resource_access/user_access_data.h>

namespace ec2
{
    class QnLayoutNotificationManager : public AbstractLayoutNotificationManager
    {
    public:
        void triggerNotification( const QnTransaction<ApiIdData>& tran, NotificationSource source);
        void triggerNotification( const QnTransaction<ApiLayoutData>& tran, NotificationSource source);
        void triggerNotification( const QnTransaction<ApiLayoutDataList>& tran, NotificationSource source);
    };

    typedef std::shared_ptr<QnLayoutNotificationManager> QnLayoutNotificationManagerPtr;

    template<class QueryProcessorType>
    class QnLayoutManager: public AbstractLayoutManager
    {
    public:
        QnLayoutManager(QueryProcessorType* const queryProcessor, const Qn::UserAccessData &userAccessData);

        virtual int getLayouts( impl::GetLayoutsHandlerPtr handler ) override;
        virtual int save(const ec2::ApiLayoutData& layout, impl::SimpleHandlerPtr handler) override;
        virtual int remove( const QnUuid& resource, impl::SimpleHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;
        Qn::UserAccessData m_userAccessData;
    };
}
