
#ifndef EC2_LAYOUT_MANAGER_H
#define EC2_LAYOUT_MANAGER_H

#include <core/resource/layout_resource.h>

#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_data.h"
#include "nx_ec/data/api_layout_data.h"
#include "transaction/transaction.h"
#include "nx_ec/data/api_conversion_functions.h"


namespace ec2
{
    class QnLayoutNotificationManager
    :
        public AbstractLayoutManager
    {
    public:
        QnLayoutNotificationManager( const ResourceContext& resCtx ) : m_resCtx( resCtx ) {}

        void triggerNotification( const QnTransaction<ApiIdData>& tran )
        {
            assert( tran.command == ApiCommand::removeLayout );
            emit removed( QnUuid(tran.params.id) );
        }

        void triggerNotification( const QnTransaction<ApiLayoutData>& tran )
        {
            assert( tran.command == ApiCommand::saveLayout);
            QnLayoutResourcePtr layoutResource(new QnLayoutResource(m_resCtx.resTypePool));
            fromApiToResource(tran.params, layoutResource);
            emit addedOrUpdated( layoutResource );
        }

        void triggerNotification( const QnTransaction<ApiLayoutDataList>& tran )
        {
            assert(tran.command == ApiCommand::saveLayouts );
            foreach(const ApiLayoutData& layout, tran.params) 
            {
                QnLayoutResourcePtr layoutResource(new QnLayoutResource(m_resCtx.resTypePool));
                fromApiToResource(layout, layoutResource);
                emit addedOrUpdated( layoutResource );
            }
        }

    protected:
        const ResourceContext m_resCtx;
    };



    template<class QueryProcessorType>
    class QnLayoutManager
    :
        public QnLayoutNotificationManager
    {
    public:
        QnLayoutManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx );

        virtual int getLayouts( impl::GetLayoutsHandlerPtr handler ) override;
        virtual int save( const QnLayoutResourceList& resources, impl::SimpleHandlerPtr handler ) override;
        virtual int remove( const QnUuid& resource, impl::SimpleHandlerPtr handler ) override;

    private:
        QnTransaction<ApiIdData> prepareTransaction( ApiCommand::Value command, const QnUuid& id );
        QnTransaction<ApiLayoutDataList> prepareTransaction( ApiCommand::Value command, const QnLayoutResourceList& layouts );

    private:
        QueryProcessorType* const m_queryProcessor;
    };
}

#endif  //EC2_LAYOUT_MANAGER_H
