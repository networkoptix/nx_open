#pragma once

#include <transaction/transaction.h>

#include <nx_ec/data/api_layout_data.h>
#include <nx_ec/managers/abstract_layout_manager.h>

namespace ec2
{
    class QnLayoutNotificationManager: public AbstractLayoutManager
    {
    public:
        QnLayoutNotificationManager( ) {}

        void triggerNotification( const QnTransaction<ApiIdData>& tran )
        {
            assert(tran.command == ApiCommand::removeLayout);
            emit removed( QnUuid(tran.params.id) );
        }

        void triggerNotification( const QnTransaction<ApiLayoutData>& tran )
        {
            assert(tran.command == ApiCommand::saveLayout);
            emit addedOrUpdated(tran.params);
        }

        void triggerNotification( const QnTransaction<ApiLayoutDataList>& tran )
        {
            assert(tran.command == ApiCommand::saveLayouts);
            for(const ApiLayoutData& layout: tran.params)
                emit addedOrUpdated(layout);
        }
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
