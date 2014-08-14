
#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_resource_data.h"
#include "transaction/transaction.h"
#include <core/resource_management/resource_pool.h>
#include "nx_ec/data/api_conversion_functions.h"


namespace ec2
{
    class QnResourceNotificationManager
    :
        public AbstractResourceManager
    {
    public:
        QnResourceNotificationManager( const ResourceContext& resCtx ) : m_resCtx( resCtx ) {}

        void triggerNotification( const QnTransaction<ApiResourceData>& tran ) {
            QnResourcePtr resource( new QnResource() );
            fromApiToResource(tran.params, resource);
            QnResourcePtr existResource = m_resCtx.pool->getResourceById(tran.params.id);
            if (existResource)
                resource->setFlags(existResource->flags());
            emit resourceChanged( resource );
        }

        void triggerNotification( const QnTransaction<ApiSetResourceStatusData>& tran ) {
            emit statusChanged( QUuid(tran.params.id), tran.params.status );
        }

        /*
        void triggerNotification( const QnTransaction<ApiSetResourceDisabledData>& tran ) {
            emit disabledChanged( QUuid(tran.params.id), tran.params.disabled );
        }
        */

        void triggerNotification( const QnTransaction<ApiResourceParamsData>& tran ) {
            QnKvPairList outData;

            for( const ApiResourceParamData& param: tran.params.params )
                outData << QnKvPair(param.name, param.value, param.predefinedParam);
            emit resourceParamsChanged( tran.params.id, outData );
        }

        void triggerNotification( const QnTransaction<ApiIdData>& tran ) {
            emit resourceRemoved( tran.params.id );
        }

    protected:
        ResourceContext m_resCtx;
    };




    template<class QueryProcessorType>
    class QnResourceManager
    :
        public QnResourceNotificationManager
    {
    public:
        QnResourceManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx );

        //!Implementation of AbstractResourceManager::getResourceTypes
        virtual int getResourceTypes( impl::GetResourceTypesHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::setResourceStatus
        virtual int setResourceStatus( const QUuid& resourceId, Qn::ResourceStatus status, impl::SetResourceStatusHandlerPtr handler ) override;
        //virtual int setResourceDisabled( const QUuid& resourceId, bool disabled, impl::SetResourceDisabledHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::getKvPairs
        virtual int getKvPairs( const QUuid &resourceId, impl::GetKvPairsHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::save
        //virtual int save( const QnResourcePtr &resource, impl::SaveResourceHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::save
        virtual int save( const QUuid& resourceId, const QnKvPairList& kvPairs, bool isPredefinedParams, impl::SaveKvPairsHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::remove
        virtual int remove( const QUuid& id, impl::SimpleHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiSetResourceStatusData> prepareTransaction( ApiCommand::Value cmd, const QUuid& id, Qn::ResourceStatus status);
        //QnTransaction<ApiSetResourceDisabledData> prepareTransaction( ApiCommand::Value command, const QUuid& id, bool disabled );
        QnTransaction<ApiResourceParamsData> prepareTransaction(ApiCommand::Value cmd, const QUuid& id, const QnKvPairList& kvPairs, bool isPredefinedParams);
        QnTransaction<ApiIdData> prepareTransaction( ApiCommand::Value cmd, const QUuid& id);
        QnTransaction<ApiResourceData> prepareTransaction( ApiCommand::Value command, const QnResourcePtr& resource );
    };
}

#endif  //RESOURCE_MANAGER_H
