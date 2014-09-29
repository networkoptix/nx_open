#ifndef __USER_MANAGER_H_
#define __USER_MANAGER_H_

#include <core/resource/user_resource.h>

#include "nx_ec/ec_api.h"
#include "transaction/transaction.h"
#include "nx_ec/data/api_user_data.h"
#include "nx_ec/data/api_conversion_functions.h"


namespace ec2
{
    class QnUserNotificationManager
    :
        public AbstractUserManager
    {
    public:
        QnUserNotificationManager( const ResourceContext& resCtx ) : m_resCtx( resCtx ) {}

        void triggerNotification( const QnTransaction<ApiUserData>& tran )
        {
            assert( tran.command == ApiCommand::saveUser);
            QnUserResourcePtr userResource(new QnUserResource());
            fromApiToResource(tran.params, userResource);
            emit addedOrUpdated( userResource );
        }

        void triggerNotification( const QnTransaction<ApiIdData>& tran )
        {
            assert( tran.command == ApiCommand::removeUser );
            emit removed( QnUuid(tran.params.id) );
        }

    protected:
        ResourceContext m_resCtx;
    };


    template<class QueryProcessorType>
    class QnUserManager
    :
        public QnUserNotificationManager
    {
    public:
        QnUserManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx );

        virtual int getUsers( impl::GetUsersHandlerPtr handler ) override;
        virtual int save( const QnUserResourcePtr& resource, impl::AddUserHandlerPtr handler ) override;
        virtual int remove( const QnUuid& id, impl::SimpleHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiUserData> prepareTransaction( ApiCommand::Value command, const QnUserResourcePtr& resource );
        QnTransaction<ApiIdData> prepareTransaction( ApiCommand::Value command, const QnUuid& resource );
    };
}

#endif  // __USER_MANAGER_H_
