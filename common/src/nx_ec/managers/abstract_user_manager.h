#pragma once

#include <nx_ec/ec_api_fwd.h>
#include <nx_ec/data/api_user_data.h>
#include <nx_ec/data/api_user_group_data.h>
#include <nx_ec/data/api_access_rights_data.h>
#include <nx_ec/impl/ec_api_impl.h>
#include <nx_ec/impl/sync_handler.h>

namespace ec2
{
    /*!
    \note All methods are asynchronous if other not specified
    */
    class AbstractUserManager: public QObject
    {
        Q_OBJECT

    public:
        virtual ~AbstractUserManager() {}

        /*!
        \param handler Functor with params: (ErrorCode, const QnUserResourceList&)
        */
        template<class TargetType, class HandlerType>
        int getUsers(TargetType* target, HandlerType handler)
        {
            return getUsers(std::static_pointer_cast<impl::GetUsersHandler>(
                std::make_shared<impl::CustomGetUsersHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode getUsersSync(ec2::ApiUserDataList* const userList)
        {
            return impl::doSyncCall<impl::GetUsersHandler>([this](impl::GetUsersHandlerPtr handler)
            {
                this->getUsers(handler);
            }, userList);
        }

        /*!
        \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType>
        int save(const ec2::ApiUserData& user, const QString& newPassword, TargetType* target, HandlerType handler)
        {
            return save(user, newPassword, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode saveSync(const ec2::ApiUserData &user, const QString& newPassword = QString())
        {
            return impl::doSyncCall<impl::SimpleHandler>([=](const impl::SimpleHandlerPtr &handler)
            {
                return this->save(user, newPassword, handler);
            });
        }

        /*!
        \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType>
        int remove(const QnUuid& id, TargetType* target, HandlerType handler) {
            return remove(id, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        /*!
        \param handler Functor with params: (ErrorCode, const QnUserResourceList&)
        */
        template<class TargetType, class HandlerType>
        int getUserGroups(TargetType* target, HandlerType handler)
        {
            return getUserGroups(std::static_pointer_cast<impl::GetUserGroupsHandler>(
                std::make_shared<impl::CustomGetUserGroupsHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode getUserGroupsSync(ec2::ApiUserGroupDataList* const result)
        {
            return impl::doSyncCall<impl::GetUserGroupsHandler>([this](impl::GetUserGroupsHandlerPtr handler)
            {
                this->getUserGroups(handler);
            }, result);
        }

        /*!
        \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType>
        int saveUserGroup(const ec2::ApiUserGroupData& user, TargetType* target, HandlerType handler)
        {
            return saveUserGroup(user, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode saveUserGroupSync(const ec2::ApiUserGroupData &data)
        {
            return impl::doSyncCall<impl::SimpleHandler>([=](const impl::SimpleHandlerPtr &handler)
            {
                return this->saveUserGroup(data, handler);
            });
        }

        /*!
        \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType>
        int removeUserGroup(const QnUuid& id, TargetType* target, HandlerType handler) {
            return removeUserGroup(id, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        /*!
        \param handler Functor with params: (ErrorCode, const QnUserResourceList&)
        */
        template<class TargetType, class HandlerType>
        int getAccessRights(TargetType* target, HandlerType handler)
        {
            return getAccessRights(std::static_pointer_cast<impl::GetAccessRightsHandler>(
                std::make_shared<impl::CustomGetAccessRightsHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode getAccessRightsSync(ec2::ApiAccessRightsDataList* const result)
        {
            return impl::doSyncCall<impl::GetAccessRightsHandler>([this](impl::GetAccessRightsHandlerPtr handler)
            {
                this->getAccessRights(handler);
            }, result);
        }

        /*!
        \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType>
        int setAccessRights(const ec2::ApiAccessRightsData& data, TargetType* target, HandlerType handler)
        {
            return setAccessRights(data, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode setAccessRightsSync(const ec2::ApiAccessRightsData &data)
        {
            return impl::doSyncCall<impl::SimpleHandler>([=](const impl::SimpleHandlerPtr &handler)
            {
                return this->setAccessRights(data, handler);
            });
        }

    signals:
        void addedOrUpdated(const ec2::ApiUserData& user);
        void groupAddedOrUpdated(const ec2::ApiUserGroupData& group);
        void removed(const QnUuid& id);
        void groupRemoved(const QnUuid& id);
        void accessRightsChanged(const ec2::ApiAccessRightsData& access);

    private:
        virtual int getUsers(impl::GetUsersHandlerPtr handler) = 0;
        virtual int save(const ec2::ApiUserData& user, const QString& newPassword, impl::SimpleHandlerPtr handler) = 0;
        virtual int remove(const QnUuid& id, impl::SimpleHandlerPtr handler) = 0;

        virtual int getUserGroups(impl::GetUserGroupsHandlerPtr handler) = 0;
        virtual int saveUserGroup(const ec2::ApiUserGroupData& group, impl::SimpleHandlerPtr handler) = 0;
        virtual int removeUserGroup(const QnUuid& id, impl::SimpleHandlerPtr handler) = 0;

        virtual int getAccessRights(impl::GetAccessRightsHandlerPtr handler) = 0;
        virtual int setAccessRights(const ec2::ApiAccessRightsData& data, impl::SimpleHandlerPtr handler) = 0;
    };

}
