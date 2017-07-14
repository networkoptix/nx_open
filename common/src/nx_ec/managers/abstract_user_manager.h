#pragma once

#include <nx_ec/ec_api_fwd.h>
#include <nx_ec/data/api_user_data.h>
#include <nx_ec/data/api_user_role_data.h>
#include <nx_ec/data/api_access_rights_data.h>
#include <nx_ec/impl/ec_api_impl.h>
#include <nx_ec/impl/sync_handler.h>

namespace ec2 {

class AbstractUserNotificationManager: public QObject
{
    Q_OBJECT

signals:
    void addedOrUpdated(const ApiUserData& user, NotificationSource source);
    void userRoleAddedOrUpdated(const ApiUserRoleData& userRole);
    void removed(const QnUuid& id);
    void userRoleRemoved(const QnUuid& id);
    void accessRightsChanged(const ApiAccessRightsData& access);
};

typedef std::shared_ptr<AbstractUserNotificationManager> AbstractUserNotificationManagerPtr;

/**
 * NOTE: All methods are asynchronous if not specified otherwise.
 */
class AbstractUserManager
{
public:
    virtual ~AbstractUserManager() {}

    /**
     * @param handler Functor with params: (ErrorCode, const QnUserResourceList&)
     */
    template<class TargetType, class HandlerType>
    int getUsers(TargetType* target, HandlerType handler)
    {
        return getUsers(std::static_pointer_cast<impl::GetUsersHandler>(
            std::make_shared<impl::CustomGetUsersHandler<TargetType, HandlerType>>(
                target, handler)));
    }

    ErrorCode getUsersSync(ApiUserDataList* const userList)
    {
        return impl::doSyncCall<impl::GetUsersHandler>(
            [this](impl::GetUsersHandlerPtr handler)
            {
                this->getUsers(handler);
            },
            userList);
    }

    /**
     * @param handler Functor with params: (ErrorCode)
     */
    template<class TargetType, class HandlerType>
    int save(const ApiUserData& user, const QString& newPassword, TargetType* target,
        HandlerType handler)
    {
        return save(user, newPassword, std::static_pointer_cast<impl::SimpleHandler>(
            std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                target, handler)));
    }

    ErrorCode saveSync(const ApiUserData& user, const QString& newPassword = QString())
    {
        return impl::doSyncCall<impl::SimpleHandler>(
            [=](const impl::SimpleHandlerPtr& handler)
            {
                return this->save(user, newPassword, handler);
            });
    }

    /**
    * @param handler Functor with params: (ErrorCode)
    */
    template<class TargetType, class HandlerType>
    int save(const ApiUserDataList& users, TargetType* target, HandlerType handler)
    {
        return save(users, std::static_pointer_cast<impl::SimpleHandler>(
            std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                target, handler)));
    }

    ErrorCode saveSync(const ApiUserDataList& users)
    {
        return impl::doSyncCall<impl::SimpleHandler>(
            [=](const impl::SimpleHandlerPtr& handler)
            {
                return this->save(users, handler);
            });
    }

    /**
     * @param handler Functor with params: (ErrorCode)
     */
    template<class TargetType, class HandlerType>
    int remove(const QnUuid& id, TargetType* target, HandlerType handler)
    {
        return remove(id, std::static_pointer_cast<impl::SimpleHandler>(
            std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                target, handler)));
    }

    ErrorCode removeSync(const QnUuid& id)
    {
        return impl::doSyncCall<impl::SimpleHandler>(
            [=](const impl::SimpleHandlerPtr& handler)
            {
                return this->remove(id, handler);
            });
    }

    /**
     * @param handler Functor with params: (ErrorCode, const QnUserResourceList&)
     */
    template<class TargetType, class HandlerType>
    int getUserRoles(TargetType* target, HandlerType handler)
    {
        return getUserRoles(std::static_pointer_cast<impl::GetUserRolesHandler>(
            std::make_shared<impl::CustomGetUserRolesHandler<TargetType, HandlerType>>(
                target, handler)));
    }

    ErrorCode getUserRolesSync(ApiUserRoleDataList* const result)
    {
        return impl::doSyncCall<impl::GetUserRolesHandler>(
            [this](impl::GetUserRolesHandlerPtr handler)
            {
                this->getUserRoles(handler);
            },
            result);
    }

    /**
     * @param handler Functor with params: (ErrorCode)
     */
    template<class TargetType, class HandlerType>
    int saveUserRole(const ApiUserRoleData& user, TargetType* target, HandlerType handler)
    {
        return saveUserRole(user, std::static_pointer_cast<impl::SimpleHandler>(
            std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                target, handler)));
    }

    ErrorCode saveUserRoleSync(const ApiUserRoleData& data)
    {
        return impl::doSyncCall<impl::SimpleHandler>(
            [=](const impl::SimpleHandlerPtr& handler)
            {
                return this->saveUserRole(data, handler);
            });
    }

    /**
     * @param handler Functor with params: (ErrorCode)
     */
    template<class TargetType, class HandlerType>
    int removeUserRole(const QnUuid& id, TargetType* target, HandlerType handler)
    {
        return removeUserRole(id, std::static_pointer_cast<impl::SimpleHandler>(
            std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                target, handler)));
    }

    /**
     * @param handler Functor with params: (ErrorCode, const QnUserResourceList&)
     */
    template<class TargetType, class HandlerType>
    int getAccessRights(TargetType* target, HandlerType handler)
    {
        return getAccessRights(std::static_pointer_cast<impl::GetAccessRightsHandler>(
            std::make_shared<impl::CustomGetAccessRightsHandler<TargetType, HandlerType>>(
                target, handler)));
    }

    ErrorCode getAccessRightsSync(ApiAccessRightsDataList* const result)
    {
        return impl::doSyncCall<impl::GetAccessRightsHandler>(
            [this](impl::GetAccessRightsHandlerPtr handler)
            {
                this->getAccessRights(handler);
            },
            result);
    }

    /**
     * @param handler Functor with params: (ErrorCode)
     */
    template<class TargetType, class HandlerType>
    int setAccessRights(const ApiAccessRightsData& data, TargetType* target, HandlerType handler)
    {
        return setAccessRights(data, std::static_pointer_cast<impl::SimpleHandler>(
            std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(
                target, handler)));
    }

    ErrorCode setAccessRightsSync(const ApiAccessRightsData& data)
    {
        return impl::doSyncCall<impl::SimpleHandler>(
            [=](const impl::SimpleHandlerPtr& handler)
            {
                return this->setAccessRights(data, handler);
            });
    }

private:
    virtual int getUsers(impl::GetUsersHandlerPtr handler) = 0;
    virtual int save(const ApiUserData& user, const QString& newPassword,
        impl::SimpleHandlerPtr handler) = 0;
    virtual int save(const ApiUserDataList& users, impl::SimpleHandlerPtr handler) = 0;
    virtual int remove(const QnUuid& id, impl::SimpleHandlerPtr handler) = 0;

    virtual int getUserRoles(impl::GetUserRolesHandlerPtr handler) = 0;
    virtual int saveUserRole(const ApiUserRoleData& userRole, impl::SimpleHandlerPtr handler) = 0;
    virtual int removeUserRole(const QnUuid& id, impl::SimpleHandlerPtr handler) = 0;

    virtual int getAccessRights(impl::GetAccessRightsHandlerPtr handler) = 0;
    virtual int setAccessRights(
        const ApiAccessRightsData& data, impl::SimpleHandlerPtr handler) = 0;
};

} // namespace ec2
