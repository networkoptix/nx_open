#pragma once

#include <transaction/transaction.h>
#include <nx_ec/managers/abstract_user_manager.h>

namespace ec2 {

class QnUserNotificationManager: public AbstractUserNotificationManager
{
public:
    QnUserNotificationManager();

    void triggerNotification(const QnTransaction<ApiUserData>& tran,
        NotificationSource source);
    void triggerNotification(const QnTransaction<ApiUserDataList>& tran,
        NotificationSource source);
    void triggerNotification(const QnTransaction<ApiIdData>& tran,
        NotificationSource source);
    void triggerNotification(const QnTransaction<ApiAccessRightsData>& tran,
        NotificationSource source);
    void triggerNotification(const QnTransaction<ApiUserRoleData>& tran,
        NotificationSource source);
};

using QnUserNotificationManagerPtr = std::shared_ptr<QnUserNotificationManager>;

template<class QueryProcessorType>
class QnUserManager: public AbstractUserManager
{
public:
    QnUserManager(QueryProcessorType* const queryProcessor, const Qn::UserAccessData &userAccessData);


    virtual int getUsers(impl::GetUsersHandlerPtr handler) override;
    virtual int save(const ec2::ApiUserData& user, const QString& newPassword,
        impl::SimpleHandlerPtr handler) override;
    virtual int save(const ec2::ApiUserDataList& users,
        impl::SimpleHandlerPtr handler) override;
    virtual int remove(const QnUuid& id, impl::SimpleHandlerPtr handler) override;

    virtual int getUserRoles(impl::GetUserRolesHandlerPtr handler) override;
    virtual int saveUserRole(const ec2::ApiUserRoleData& userRole,
        impl::SimpleHandlerPtr handler) override;
    virtual int removeUserRole(const QnUuid& id, impl::SimpleHandlerPtr handler) override;

    virtual int getAccessRights(impl::GetAccessRightsHandlerPtr handler) override;
    virtual int setAccessRights(const ec2::ApiAccessRightsData& data,
        impl::SimpleHandlerPtr handler) override;
private:
    QueryProcessorType* const m_queryProcessor;
    Qn::UserAccessData m_userAccessData;
};

} // namespace ec2
