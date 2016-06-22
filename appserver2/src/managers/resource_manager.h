
#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_resource_data.h"
#include "transaction/transaction.h"
#include <core/resource_management/resource_pool.h>
#include "nx_ec/data/api_conversion_functions.h"


namespace ec2
{
    class QnResourceNotificationManager : public AbstractResourceNotificationManager
    {
    public:
        QnResourceNotificationManager() {}

        void triggerNotification( const QnTransaction<ApiResourceStatusData>& tran ) {
            emit statusChanged( QnUuid(tran.params.id), tran.params.status );
        }

        void triggerNotification( const QnTransaction<ApiLicenseOverflowData>& /*tran*/ ) {
            // nothing to do
        }

        void triggerNotification( const QnTransaction<ApiResourceParamWithRefData>& tran ) {
            if (tran.command == ApiCommand::setResourceParam)
                emit resourceParamChanged(tran.params);
            else if (tran.command == ApiCommand::removeResourceParam)
                emit resourceParamRemoved(tran.params);
        }

        void triggerNotification( const QnTransaction<ApiResourceParamWithRefDataList>& tran ) {
            for (const ec2::ApiResourceParamWithRefData& param : tran.params)
            {
                if (tran.command == ApiCommand::setResourceParams)
                    emit resourceParamChanged(param);
                else if (tran.command == ApiCommand::removeResourceParams)
                    emit resourceParamRemoved(param);
            }
        }

        void triggerNotification( const QnTransaction<ApiIdData>& tran ) {
            emit resourceRemoved( tran.params.id );
        }

        void triggerNotification( const QnTransaction<ApiIdDataList>& tran ) {
            for(const ApiIdData& id: tran.params)
                emit resourceRemoved( id.id );
        }
    };

    typedef std::shared_ptr<QnResourceNotificationManager> QnResourceNotificationManagerPtr;


    template<class QueryProcessorType>
    class QnResourceManager : public AbstractResourceManager
    {
    public:
        QnResourceManager(QueryProcessorType* const queryProcessor, const Qn::UserAccessData &userAccessData);

        //!Implementation of AbstractResourceManager::getResourceTypes
        virtual int getResourceTypes( impl::GetResourceTypesHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::setResourceStatus
        virtual int setResourceStatus( const QnUuid& resourceId, Qn::ResourceStatus status, impl::SetResourceStatusHandlerPtr handler ) override;
        virtual int setResourceStatusLocal( const QnUuid& resourceId, Qn::ResourceStatus status, impl::SetResourceStatusHandlerPtr handler ) override;
        //virtual int setResourceDisabled( const QnUuid& resourceId, bool disabled, impl::SetResourceDisabledHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::getKvPairs
        virtual int getKvPairs( const QnUuid &resourceId, impl::GetKvPairsHandlerPtr handler ) override;

        //!Implementation of AbstractResourceManager::getStatusList
        virtual int getStatusList( const QnUuid &resourceId, impl::GetStatusListHandlerPtr handler ) override;

        //!Implementation of AbstractResourceManager::save
        virtual int save(const ec2::ApiResourceParamWithRefDataList& kvPairs, impl::SaveKvPairsHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::remove
        virtual int remove( const QnUuid& id, impl::SimpleHandlerPtr handler ) override;
        virtual int remove( const QVector<QnUuid>& idList, impl::SimpleHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;
        Qn::UserAccessData m_userAccessData;

        QnTransaction<ApiResourceStatusData> prepareTransaction( ApiCommand::Value cmd, const QnUuid& id, Qn::ResourceStatus status);
        QnTransaction<ApiResourceParamWithRefDataList> prepareTransaction(ApiCommand::Value cmd, const ec2::ApiResourceParamWithRefDataList& kvPairs);
        QnTransaction<ApiIdData> prepareTransaction( ApiCommand::Value cmd, const QnUuid& id);
    };
}

#endif  //RESOURCE_MANAGER_H
