#ifndef __USER_MANAGER_H_
#define __USER_MANAGER_H_

#include <core/resource/user_resource.h>

#include "nx_ec/ec_api.h"
#include "transaction/transaction.h"
#include "nx_ec/data/api_user_data.h"
#include "nx_ec/data/api_conversion_functions.h"


namespace ec2
{
    template<class QueryProcessorType>
    class QnUserManager
    :
        public AbstractUserManager
    {
    public:
        QnUserManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx );

        virtual int getUsers( impl::GetUsersHandlerPtr handler ) override;
        virtual int save( const QnUserResourcePtr& resource, impl::AddUserHandlerPtr handler ) override;
        virtual int remove( const QnId& id, impl::SimpleHandlerPtr handler ) override;

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
            emit removed( QnId(tran.params.id) );
        }

    private:
        QueryProcessorType* const m_queryProcessor;
        ResourceContext m_resCtx;

        QnTransaction<ApiUserData> prepareTransaction( ApiCommand::Value command, const QnUserResourcePtr& resource );
        QnTransaction<ApiIdData> prepareTransaction( ApiCommand::Value command, const QnId& resource );
    };
}

#endif  // __USER_MANAGER_H_
