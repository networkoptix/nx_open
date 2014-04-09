
#ifndef EC2_VIDEOWALL_MANAGER_H
#define EC2_VIDEOWALL_MANAGER_H

#include <core/resource/videowall_resource.h>

#include "nx_ec/ec_api.h"
#include "transaction/transaction.h"
#include "nx_ec/data/ec2_videowall_data.h"

namespace ec2
{
    template<class QueryProcessorType>
    class QnVideowallManager
        :
        public AbstractVideowallManager
    {
    public:
        QnVideowallManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx );

        virtual int getVideowalls( impl::GetVideowallsHandlerPtr handler ) override;
        virtual int save( const QnVideoWallResourcePtr& resource, impl::AddVideowallHandlerPtr handler ) override;
        virtual int remove( const QnId& id, impl::SimpleHandlerPtr handler ) override;

        void triggerNotification( const QnTransaction<ApiVideowallData>& tran )
        {
            assert( tran.command == ApiCommand::saveVideowall);
            QnVideoWallResourcePtr VideoWallResource(new QnVideoWallResource());
            tran.params.toResource( VideoWallResource );
            emit addedOrUpdated( VideoWallResource );
        }

        void triggerNotification( const QnTransaction<ApiIdData>& tran )
        {
            assert( tran.command == ApiCommand::removeVideowall );
            emit removed( QnId(tran.params.id) );
        }

    private:
        QueryProcessorType* const m_queryProcessor;
        ResourceContext m_resCtx;

        QnTransaction<ApiVideowallData> prepareTransaction( ApiCommand::Value command, const QnVideoWallResourcePtr& resource );
        QnTransaction<ApiIdData> prepareTransaction( ApiCommand::Value command, const QnId& resource );
    };
}


#endif  //EC2_VIDEOWALL_MANAGER_H
