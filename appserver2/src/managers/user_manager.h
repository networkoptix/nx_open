#ifndef __USER_MANAGER_H_
#define __USER_MANAGER_H_

#include <core/resource/user_resource.h>

#include "transaction/transaction.h"
#include "nx_ec/data/api_user_data.h"
#include "nx_ec/data/api_conversion_functions.h"
#include <nx_ec/managers/abstract_user_manager.h>

namespace ec2
{
    class QnUserNotificationManager: public AbstractUserManager
    {
    public:
        QnUserNotificationManager( ) {}

        void triggerNotification( const QnTransaction<ApiUserData>& tran )
        {
            assert( tran.command == ApiCommand::saveUser);
            emit addedOrUpdated(tran.params);
        }

        void triggerNotification( const QnTransaction<ApiIdData>& tran )
        {
            assert( tran.command == ApiCommand::removeUser );
            emit removed( QnUuid(tran.params.id) );
        }
    };


    template<class QueryProcessorType>
    class QnUserManager: public QnUserNotificationManager
    {
    public:
        QnUserManager( QueryProcessorType* const queryProcessor);

        virtual int getUsers(impl::GetUsersHandlerPtr handler ) override;
        virtual int save( const ec2::ApiUserData& user, const QString& newPassword, impl::AddUserHandlerPtr handler ) override;
        virtual int remove( const QnUuid& id, impl::SimpleHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiUserData> prepareTransaction( ApiCommand::Value command, const ec2::ApiUserData& user);
        QnTransaction<ApiIdData> prepareTransaction( ApiCommand::Value command, const QnUuid& resource );
    };
}

#endif  // __USER_MANAGER_H_
