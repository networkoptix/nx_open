#pragma once

#include <nx_ec/ec_api_fwd.h>
#include <nx_ec/data/api_user_data.h>
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
        int addAccess(const ec2::ApiAccessRightsData& access, TargetType* target, HandlerType handler)
        {
            return addAccess(access, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode addAccessSync(const ec2::ApiAccessRightsData &access)
        {
            return impl::doSyncCall<impl::SimpleHandler>([=](const impl::SimpleHandlerPtr &handler)
            {
                return this->addAccess(access, handler);
            });
        }

        /*!
        \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType>
        int removeAccess(const ec2::ApiAccessRightsData& access, TargetType* target, HandlerType handler)
        {
            return removeAccess(access, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode removeAccessSync(const ec2::ApiAccessRightsData &access)
        {
            return impl::doSyncCall<impl::SimpleHandler>([=](const impl::SimpleHandlerPtr &handler)
            {
                return this->removeAccess(access, handler);
            });
        }

    signals:
        void addedOrUpdated(const ec2::ApiUserData& user);
        void removed(const QnUuid& id);
        void accessAdded(const ec2::ApiAccessRightsData& access);
        void accessRemoved(const ec2::ApiAccessRightsData& access);

    private:
        virtual int getUsers(impl::GetUsersHandlerPtr handler) = 0;
        virtual int save(const ec2::ApiUserData& user, const QString& newPassword, impl::SimpleHandlerPtr handler) = 0;
        virtual int remove(const QnUuid& id, impl::SimpleHandlerPtr handler) = 0;
        virtual int getAccessRights(impl::GetAccessRightsHandlerPtr handler) = 0;
        virtual int addAccess(const ec2::ApiAccessRightsData& access, impl::SimpleHandlerPtr handler) = 0;
        virtual int removeAccess(const ec2::ApiAccessRightsData& access, impl::SimpleHandlerPtr handler) = 0;
    };

}
