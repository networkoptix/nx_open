
#ifndef EC2_LAYOUT_MANAGER_H
#define EC2_LAYOUT_MANAGER_H

#include <core/resource/layout_resource.h>

#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_data.h"
#include "nx_ec/data/api_layout_data.h"
#include "transaction/transaction.h"


namespace ec2
{
    template<class QueryProcessorType>
    class QnLayoutManager
    :
        public AbstractLayoutManager
    {
    public:
        QnLayoutManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx );

        virtual int getLayouts( impl::GetLayoutsHandlerPtr handler ) override;
        virtual int save( const QnLayoutResourceList& resources, impl::SimpleHandlerPtr handler ) override;
        virtual int remove( const QnId& resource, impl::SimpleHandlerPtr handler ) override;

        void triggerNotification( const QnTransaction<ApiIdData>& tran )
        {
            assert( tran.command == ApiCommand::removeLayout );
            emit removed( QnId(tran.params.id) );
        }

        void triggerNotification( const QnTransaction<ApiLayoutData>& tran )
        {
            assert( tran.command == ApiCommand::saveLayout);
            QnLayoutResourcePtr layoutResource(new QnLayoutResource());
            fromApiToResource(tran.params, layoutResource);
            emit addedOrUpdated( layoutResource );
        }

        void triggerNotification( const QnTransaction<ApiLayoutDataList>& tran )
        {
            assert(tran.command == ApiCommand::saveLayouts );
            foreach(const ApiLayoutData& layout, tran.params.data) 
            {
                QnLayoutResourcePtr layoutResource(new QnLayoutResource());
                fromApiToResource(layout, layoutResource);
                emit addedOrUpdated( layoutResource );
            }
        }

    private:
        QnTransaction<ApiIdData> prepareTransaction( ApiCommand::Value command, const QnId& id );
        QnTransaction<ApiLayoutDataList> prepareTransaction( ApiCommand::Value command, const QnLayoutResourceList& layouts );

    private:
        QueryProcessorType* const m_queryProcessor;
        const ResourceContext m_resCtx;
    };
}

#endif  //EC2_LAYOUT_MANAGER_H
