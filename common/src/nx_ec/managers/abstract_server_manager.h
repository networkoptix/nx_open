#pragma once

#include <nx_ec/ec_api_fwd.h>
#include <nx_ec/data/api_media_server_data.h>
#include <nx_ec/impl/ec_api_impl.h>
#include <nx_ec/impl/sync_handler.h>

namespace ec2
{

class AbstractMediaServerNotificationManager : public QObject
{
    Q_OBJECT
public:
signals:
    void addedOrUpdated(const ec2::ApiMediaServerData& server, NotificationSource source);
    void storageChanged(const ec2::ApiStorageData& storage, NotificationSource source);
    void removed(const QnUuid& id);
    void storageRemoved(const QnUuid& id);
    void userAttributesChanged(const ec2::ApiMediaServerUserAttributesData& attributes);
    void userAttributesRemoved(const QnUuid& id);
};

typedef std::shared_ptr<AbstractMediaServerNotificationManager> AbstractMediaServerNotificationManagerPtr;

    class AbstractMediaServerManager
    {
    public:
        virtual ~AbstractMediaServerManager() {}

        /*!
        \param handler Functor with params: (ErrorCode, const QnMediaServerResourceList& servers)
        */
        template<class TargetType, class HandlerType>
        int getServers(TargetType* target, HandlerType handler)
        {
            return getServers(std::static_pointer_cast<impl::GetServersHandler>(
                std::make_shared<impl::CustomGetServersHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode getServersSync(ec2::ApiMediaServerDataList* const serverList)
        {
            return impl::doSyncCall<impl::GetServersHandler>([this](const impl::GetServersHandlerPtr &handler)
            {
                return this->getServers(handler);
            }, serverList );
        }

        ErrorCode getServersExSync(ec2::ApiMediaServerDataExList* const serverList)
        {
            return impl::doSyncCall<impl::GetServersExHandler>([this](const impl::GetServersExHandlerPtr &handler)
            {
                return this->getServersEx(handler);
            }, serverList);
        }

        /*!
        \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType>
        int save(const ec2::ApiMediaServerData& server, TargetType* target, HandlerType handler)
        {
            return save(server, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode saveSync(const ec2::ApiMediaServerData& server)
        {
            return impl::doSyncCall<impl::SimpleHandler>([this, server](const impl::SimpleHandlerPtr &handler)
            {
                return this->save(server, handler);
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

        template<class TargetType, class HandlerType>
        int remove(const QVector<QnUuid>& idList, TargetType* target, HandlerType handler) {
            return remove(idList, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode removeSync(const QnUuid& id)
        {
            return impl::doSyncCall<impl::SimpleHandler>([=](const impl::SimpleHandlerPtr &handler)
            {
                return this->remove(id, handler);
            });
        }

        /*!
        \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType>
        int saveUserAttributes(const ec2::ApiMediaServerUserAttributesDataList& serverAttrs, TargetType* target, HandlerType handler)
        {
            return saveUserAttributes(serverAttrs, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode saveUserAttributesSync(const ec2::ApiMediaServerUserAttributesDataList& serverAttrs)
        {
            return impl::doSyncCall<impl::SimpleHandler>([=](const impl::SimpleHandlerPtr &handler)
            {
                return this->saveUserAttributes(serverAttrs, handler);
            });
        }


        /*!
        \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType>
        int saveStorages(const ec2::ApiStorageDataList& storages, TargetType* target, HandlerType handler)
        {
            return saveStorages(storages, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode saveStoragesSync(const ec2::ApiStorageDataList& storages)
        {
            return impl::doSyncCall<impl::SimpleHandler>([=](const impl::SimpleHandlerPtr &handler)
            {
                return this->saveStorages(storages, handler);
            });
        }

        /*!
        \param handler Functor with params: (ErrorCode)
        */
        template<class TargetType, class HandlerType>
        int removeStorages(const ec2::ApiIdDataList& storages, TargetType* target, HandlerType handler)
        {
            return removeStorages(storages, std::static_pointer_cast<impl::SimpleHandler>(
                std::make_shared<impl::CustomSimpleHandler<TargetType, HandlerType>>(target, handler)));
        }

        ErrorCode removeStoragesSync(const ec2::ApiIdDataList& storages)
        {
            return impl::doSyncCall<impl::SimpleHandler>([=](const impl::SimpleHandlerPtr &handler)
            {
                return this->removeStorages(storages, handler);
            });
        }

        /*!
        \param mediaServerId if not NULL, returned list contains at most one element: the one, corresponding to \a mediaServerId.
        If NULL, returned list contains data of all known servers
        \param handler Functor with params: (ErrorCode, const QnMediaServerUserAttributesList& serverUserAttributesList)
        */
        template<class TargetType, class HandlerType>
        int getUserAttributes(const QnUuid& mediaServerId, TargetType* target, HandlerType handler)
        {
            return getUserAttributes(mediaServerId, std::static_pointer_cast<impl::GetServerUserAttributesHandler>(
                std::make_shared<impl::CustomGetServerUserAttributesHandler<TargetType, HandlerType>>(target, handler)));
        }

        /*!
        \param mediaServerId if not NULL, returned list contains at most one element: the one, corresponding to \a mediaServerId.
        If NULL, returned list contains data of all known servers
        */
        ErrorCode getUserAttributesSync(const QnUuid& mediaServerId, ec2::ApiMediaServerUserAttributesDataList* const serverAttrsList)
        {
            return impl::doSyncCall<impl::GetServerUserAttributesHandler>([=](const impl::GetServerUserAttributesHandlerPtr &handler)
            {
                return this->getUserAttributes(mediaServerId, handler);
            }, serverAttrsList);
        }

        /*!
        \param mediaServerId if not NULL, returned list contains at most one element: the one, corresponding to \a mediaServerId.
        If NULL, returned list contains data of all known servers
        \param handler Functor with params: (ErrorCode, const QnResourceList& storages)
        */
        template<class TargetType, class HandlerType>
        int getStorages(const QnUuid& mediaServerId, TargetType* target, HandlerType handler)
        {
            return getStorages(mediaServerId, std::static_pointer_cast<impl::GetStoragesHandler>(
                std::make_shared<impl::CustomGetStoragesHandler<TargetType, HandlerType>>(target, handler)));
        }

        /*!
        \param mediaServerId if not NULL, returned list contains at most one element: the one, corresponding to \a mediaServerId.
        If NULL, returned list contains data of all known servers
        */
        ErrorCode getStoragesSync(const QnUuid& mediaServerId, ec2::ApiStorageDataList* const storages)
        {
            return impl::doSyncCall<impl::GetStoragesHandler>([=](const impl::GetStoragesHandlerPtr &handler)
            {
                return this->getStorages(mediaServerId, handler);
            }, storages);
        }

    protected:
        virtual int getServers(impl::GetServersHandlerPtr handler) = 0;
        virtual int getServersEx(impl::GetServersExHandlerPtr handler) = 0;
        virtual int save(const ec2::ApiMediaServerData&, impl::SimpleHandlerPtr handler) = 0;
        virtual int remove(const QnUuid& id, impl::SimpleHandlerPtr handler) = 0;
        virtual int saveUserAttributes(const ec2::ApiMediaServerUserAttributesDataList& serverAttrs, impl::SimpleHandlerPtr handler) = 0;
        virtual int saveStorages(const ec2::ApiStorageDataList& storages, impl::SimpleHandlerPtr handler) = 0;
        virtual int removeStorages(const ec2::ApiIdDataList& storages, impl::SimpleHandlerPtr handler) = 0;
        virtual int getUserAttributes(const QnUuid& mediaServerId, impl::GetServerUserAttributesHandlerPtr handler) = 0;
        virtual int getStorages(const QnUuid& mediaServerId, impl::GetStoragesHandlerPtr handler) = 0;
    };


}
