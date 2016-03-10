#pragma once

#include <transaction/transaction.h>

#include <nx_ec/data/api_layout_data.h>
#include <nx_ec/managers/abstract_layout_manager.h>

namespace ec2
{
    class QnLayoutNotificationManager: public AbstractLayoutManager
    {
    public:
        QnLayoutNotificationManager( );

        void triggerNotification( const QnTransaction<ApiIdData>& tran );
        void triggerNotification( const QnTransaction<ApiLayoutData>& tran );
        void triggerNotification( const QnTransaction<ApiLayoutDataList>& tran );
    };



    template<class QueryProcessorType>
    class QnLayoutManager: public QnLayoutNotificationManager
    {
    public:
        QnLayoutManager( QueryProcessorType* const queryProcessor );

        virtual int getLayouts( impl::GetLayoutsHandlerPtr handler ) override;
        virtual int save(const ec2::ApiLayoutData& layout, impl::SimpleHandlerPtr handler) override;
        virtual int remove( const QnUuid& resource, impl::SimpleHandlerPtr handler ) override;

    private:
        QnTransaction<ApiIdData> prepareTransaction( ApiCommand::Value command, const QnUuid& id );
        QnTransaction<ApiLayoutData> prepareTransaction( ApiCommand::Value command, const ec2::ApiLayoutData& layout);

    private:
        QueryProcessorType* const m_queryProcessor;
    };
}
