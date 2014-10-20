
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
            fromApiToResource(tran.params, resource.data());
            QnResourcePtr existResource = m_resCtx.pool->getResourceById(tran.params.id);
            if (existResource)
                resource->setFlags(existResource->flags());
            emit resourceChanged( std::move(resource ));
        }

        void triggerNotification( const QnTransaction<ApiResourceStatusData>& tran ) {
            emit statusChanged( QnUuid(tran.params.id), tran.params.status );
        }

        /*
        void triggerNotification( const QnTransaction<ApiSetResourceDisabledData>& tran ) {
            emit disabledChanged( QnUuid(tran.params.id), tran.params.disabled );
        }
        */

        void triggerNotification( const QnTransaction<ApiResourceParamWithRefData>& tran ) {
            emit resourceParamChanged(tran.params );
        }

        void triggerNotification( const QnTransaction<ApiResourceParamWithRefDataList>& tran ) {
            foreach(const ec2::ApiResourceParamWithRefData& param, tran.params)
                emit resourceParamChanged(param);
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
        virtual int setResourceStatus( const QnUuid& resourceId, Qn::ResourceStatus status, impl::SetResourceStatusHandlerPtr handler ) override;
        //virtual int setResourceDisabled( const QnUuid& resourceId, bool disabled, impl::SetResourceDisabledHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::getKvPairs
        virtual int getKvPairs( const QnUuid &resourceId, impl::GetKvPairsHandlerPtr handler ) override;

        //!Implementation of AbstractResourceManager::getStatusList
        virtual int getStatusList( const QnUuid &resourceId, impl::GetStatusListHandlerPtr handler ) override;

        //!Implementation of AbstractResourceManager::save
        //virtual int save( const QnResourcePtr &resource, impl::SaveResourceHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::save
        virtual int save(const ec2::ApiResourceParamWithRefDataList& kvPairs, impl::SaveKvPairsHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::removeParams
        virtual int removeParams(const ec2::ApiResourceParamWithRefDataList& kvPairs, impl::SaveKvPairsHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::remove
        virtual int remove( const QnUuid& id, impl::SimpleHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiResourceStatusData> prepareTransaction( ApiCommand::Value cmd, const QnUuid& id, Qn::ResourceStatus status);
        //QnTransaction<ApiSetResourceDisabledData> prepareTransaction( ApiCommand::Value command, const QnUuid& id, bool disabled );
        QnTransaction<ApiResourceParamWithRefDataList> prepareTransaction(ApiCommand::Value cmd, const ec2::ApiResourceParamWithRefDataList& kvPairs);
        QnTransaction<ApiIdData> prepareTransaction( ApiCommand::Value cmd, const QnUuid& id);
        QnTransaction<ApiResourceData> prepareTransaction( ApiCommand::Value command, const QnResourcePtr& resource );
    };
}

#endif  //RESOURCE_MANAGER_H
