#pragma once

#include <transaction/transaction.h>
#include <nx_ec/managers/abstract_user_manager.h>

namespace ec2
{
    class QnUserNotificationManager: public AbstractUserNotificationManager
    {
    public:
        QnUserNotificationManager( );

        void triggerNotification(const QnTransaction<ApiUserData>& tran);
        void triggerNotification(const QnTransaction<ApiIdData>& tran);
        void triggerNotification(const QnTransaction<ApiAccessRightsData>& tran);
        void triggerNotification(const QnTransaction<ApiUserGroupData>& tran);
    };

    typedef std::shared_ptr<QnUserNotificationManager> QnUserNotificationManagerPtr;

    template<class QueryProcessorType>
    class QnUserManager: public AbstractUserManager
    {
    public:
        QnUserManager(QueryProcessorType* const queryProcessor, const Qn::UserAccessData &userAccessData);


        virtual int getUsers(impl::GetUsersHandlerPtr handler ) override;
        virtual int save( const ec2::ApiUserData& user, const QString& newPassword, impl::SimpleHandlerPtr handler ) override;
        virtual int remove( const QnUuid& id, impl::SimpleHandlerPtr handler ) override;

        virtual int getUserGroups(impl::GetUserGroupsHandlerPtr handler) override;
        virtual int saveUserGroup(const ec2::ApiUserGroupData& group, impl::SimpleHandlerPtr handler) override;
        virtual int removeUserGroup(const QnUuid& id, impl::SimpleHandlerPtr handler) override;

        virtual int getAccessRights(impl::GetAccessRightsHandlerPtr handler) override;
        virtual int setAccessRights(const ec2::ApiAccessRightsData& data, impl::SimpleHandlerPtr handler) override;
    private:
        QueryProcessorType* const m_queryProcessor;
        Qn::UserAccessData m_userAccessData;
    };

}
