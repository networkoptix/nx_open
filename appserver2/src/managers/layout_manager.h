#pragma once

#include <transaction/transaction.h>

#include <nx_ec/data/api_layout_data.h>
#include <nx_ec/managers/abstract_layout_manager.h>
#include <core/resource_management/user_access_data.h>

namespace ec2
{
    class QnLayoutNotificationManager: public AbstractLayoutManagerBase
    {
    public:
        QnLayoutNotificationManager( );

        void triggerNotification( const QnTransaction<ApiIdData>& tran );
        void triggerNotification( const QnTransaction<ApiLayoutData>& tran );
        void triggerNotification( const QnTransaction<ApiLayoutDataList>& tran );
    };

    typedef std::shared_ptr<QnLayoutNotificationManager> QnLayoutNotificationManagerPtr;
    typedef QnLayoutNotificationManager *QnLayoutNotificationManagerRawPtr;

    template<class QueryProcessorType>
    class QnLayoutManager: public AbstractLayoutManager
    {
    public:
        QnLayoutManager(QnLayoutNotificationManagerRawPtr base,
                        QueryProcessorType* const queryProcessor,
                        const Qn::UserAccessData &userAccessData);

        QnLayoutNotificationManagerRawPtr getBase() const override { return m_base; }

        virtual int getLayouts( impl::GetLayoutsHandlerPtr handler ) override;
        virtual int save(const ec2::ApiLayoutData& layout, impl::SimpleHandlerPtr handler) override;
        virtual int remove( const QnUuid& resource, impl::SimpleHandlerPtr handler ) override;

    private:
        QnLayoutNotificationManagerRawPtr m_base;
        QueryProcessorType* const m_queryProcessor;
        Qn::UserAccessData m_userAccessData;
    };
}
