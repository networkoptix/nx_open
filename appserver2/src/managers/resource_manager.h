
#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include "nx_ec/ec_api.h"
#include "nx_ec/data/api_resource_data.h"
#include "transaction/transaction.h"
#include <core/resource_management/resource_pool.h>
#include "nx_ec/data/api_conversion_functions.h"
#include <nx/utils/log/log.h>


namespace ec2
{
    class QnResourceNotificationManager : public AbstractResourceNotificationManager
    {
    public:
        QnResourceNotificationManager() {}

        void triggerNotification( const QnTransaction<ApiResourceStatusData>& tran, NotificationSource source)
        {
            NX_LOG(lit("%1 Emit statusChanged signal for resource %2")
                    .arg(QString::fromLatin1(Q_FUNC_INFO))
                    .arg(tran.params.id.toString()), cl_logDEBUG2);
            emit statusChanged( QnUuid(tran.params.id), tran.params.status, source);
        }

        void triggerNotification( const QnTransaction<ApiLicenseOverflowData>& /*tran*/, NotificationSource /*source*/) {
            // nothing to do
        }

        void triggerNotification(const QnTransaction<ApiCleanupDatabaseData>& /*tran*/, NotificationSource /*source*/) {
            // nothing to do
        }

        void triggerNotification( const QnTransaction<ApiResourceParamWithRefData>& tran, NotificationSource /*source*/) {
            if (tran.command == ApiCommand::setResourceParam)
                emit resourceParamChanged(tran.params);
            else if (tran.command == ApiCommand::removeResourceParam)
                emit resourceParamRemoved(tran.params);
        }

        void triggerNotification( const QnTransaction<ApiResourceParamWithRefDataList>& tran, NotificationSource /*source*/) {
            for (const ec2::ApiResourceParamWithRefData& param : tran.params)
            {
                if (tran.command == ApiCommand::setResourceParams)
                    emit resourceParamChanged(param);
                else if (tran.command == ApiCommand::removeResourceParams)
                    emit resourceParamRemoved(param);
            }
        }

        void triggerNotification( const QnTransaction<ApiIdData>& tran, NotificationSource /*source*/)
        {
            if (tran.command == ApiCommand::removeResourceStatus)
                emit resourceStatusRemoved(tran.params.id);
            else
                emit resourceRemoved(tran.params.id);
        }

        void triggerNotification( const QnTransaction<ApiIdDataList>& tran, NotificationSource /*source*/) {
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
        //virtual int setResourceDisabled( const QnUuid& resourceId, bool disabled, impl::SetResourceDisabledHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::getKvPairs
        virtual int getKvPairs( const QnUuid &resourceId, impl::GetKvPairsHandlerPtr handler ) override;

        //!Implementation of AbstractResourceManager::getStatusList
        virtual int getStatusList( const QnUuid &resourceId, impl::GetStatusListHandlerPtr handler ) override;

        //!Implementation of AbstractResourceManager::save
        virtual int save(const ec2::ApiResourceParamWithRefDataList& kvPairs, impl::SimpleHandlerPtr handler) override;
        //!Implementation of AbstractResourceManager::remove
        virtual int remove( const QnUuid& id, impl::SimpleHandlerPtr handler ) override;
        virtual int remove( const QVector<QnUuid>& idList, impl::SimpleHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;
        Qn::UserAccessData m_userAccessData;
    };
}

#endif  //RESOURCE_MANAGER_H
