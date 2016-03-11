#pragma once

#include <transaction/transaction.h>
#include <nx_ec/managers/abstract_user_manager.h>

namespace ec2
{
    class QnUserNotificationManager: public AbstractUserManager
    {
    public:
        QnUserNotificationManager( );

        void triggerNotification( const QnTransaction<ApiUserData>& tran );
        void triggerNotification( const QnTransaction<ApiIdData>& tran );
    };


    template<class QueryProcessorType>
    class QnUserManager: public QnUserNotificationManager
    {
    public:
        QnUserManager( QueryProcessorType* const queryProcessor);

        virtual int getUsers(impl::GetUsersHandlerPtr handler ) override;
        virtual int save( const ec2::ApiUserData& user, const QString& newPassword, impl::SimpleHandlerPtr handler ) override;
        virtual int remove( const QnUuid& id, impl::SimpleHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiUserData> prepareTransaction( ApiCommand::Value command, const ec2::ApiUserData& user);
        QnTransaction<ApiIdData> prepareTransaction( ApiCommand::Value command, const QnUuid& resource );
    };
}
