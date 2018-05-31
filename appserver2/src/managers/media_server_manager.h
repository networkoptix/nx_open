#pragma once

#include <transaction/transaction.h>

#include <nx_ec/managers/abstract_server_manager.h>

namespace ec2
{

    template<class QueryProcessorType>
    class QnMediaServerManager: public AbstractMediaServerManager
    {
    public:
        QnMediaServerManager(QueryProcessorType* const queryProcessor, const Qn::UserAccessData &userAccessData);

        //!Implementation of QnMediaServerManager::getServers
        virtual int getServers(impl::GetServersHandlerPtr handler) override;
        virtual int getServersEx(impl::GetServersExHandlerPtr handler) override;
        //!Implementation of QnMediaServerManager::saveServer
        virtual int save( const ec2::ApiMediaServerData&, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::remove
        virtual int remove( const QnUuid& id, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::saveUserAttributes
        virtual int saveUserAttributes( const ec2::ApiMediaServerUserAttributesDataList& serverAttrs, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::saveStorages
        virtual int saveStorages( const ec2::ApiStorageDataList& storages, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::removeStorages
        virtual int removeStorages( const nx::vms::api::IdDataList& storages, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::getUserAttributes
        virtual int getUserAttributes( const QnUuid& mediaServerId, impl::GetServerUserAttributesHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::getStorages
        virtual int getStorages( const QnUuid& mediaServerId, impl::GetStoragesHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;
        Qn::UserAccessData m_userAccessData;
    };

    
    template<class QueryProcessorType>
    QnMediaServerManager<QueryProcessorType>::QnMediaServerManager(QueryProcessorType* const queryProcessor, const Qn::UserAccessData &userAccessData)
    :
      m_queryProcessor( queryProcessor ),
      m_userAccessData(userAccessData)
    {}

    template<class T>
    int QnMediaServerManager<T>::getServers(impl::GetServersHandlerPtr handler )
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ec2::ApiMediaServerDataList& servers) {
            handler->done( reqID, errorCode, servers);
        };
        m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<QnUuid, ApiMediaServerDataList, decltype(queryDoneHandler)> (
            ApiCommand::getMediaServers, QnUuid(), queryDoneHandler);
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::getServersEx(impl::GetServersExHandlerPtr handler)
    {
        const int reqID = generateRequestID();

        auto queryDoneHandler = [reqID, handler, this](ErrorCode errorCode, const ec2::ApiMediaServerDataExList& servers) {
            handler->done(reqID, errorCode, servers);
        };
        m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<QnUuid, ApiMediaServerDataExList, decltype(queryDoneHandler)>(
            ApiCommand::getMediaServersEx, QnUuid(), queryDoneHandler);
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::save(const ec2::ApiMediaServerData& server, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::saveMediaServer,
            server, [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::remove( const QnUuid& id, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::removeMediaServer, nx::vms::api::IdData(id),
            [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::saveUserAttributes(const ec2::ApiMediaServerUserAttributesDataList& serverAttrs, impl::SimpleHandlerPtr handler)
    {
        const int reqID = generateRequestID();
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::saveMediaServerUserAttributesList, serverAttrs,
            [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::saveStorages( const ec2::ApiStorageDataList& storages, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::saveStorages, storages,
            [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::removeStorages( const nx::vms::api::IdDataList& storages, impl::SimpleHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        m_queryProcessor->getAccess(m_userAccessData).processUpdateAsync(
            ApiCommand::removeStorages, storages,
            [handler, reqID](ec2::ErrorCode errorCode)
        {
            handler->done(reqID, errorCode);
        });
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::getUserAttributes( const QnUuid& mediaServerId, impl::GetServerUserAttributesHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ApiMediaServerUserAttributesDataList& serverUserAttributesList ) {
            handler->done( reqID, errorCode, serverUserAttributesList);
        };
        m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<QnUuid, ApiMediaServerUserAttributesDataList, decltype(queryDoneHandler)>
            ( ApiCommand::getMediaServerUserAttributesList, mediaServerId, queryDoneHandler );
        return reqID;
    }

    template<class T>
    int QnMediaServerManager<T>::getStorages( const QnUuid& mediaServerId, impl::GetStoragesHandlerPtr handler )
    {
        const int reqID = generateRequestID();
        auto queryDoneHandler = [reqID, handler, this]( ErrorCode errorCode, const ec2::ApiStorageDataList& storages )
        {
            handler->done( reqID, errorCode, storages );
        };
        m_queryProcessor->getAccess(m_userAccessData).template processQueryAsync<ParentId, ec2::ApiStorageDataList, decltype(queryDoneHandler)>
            ( ApiCommand::getStorages, mediaServerId, queryDoneHandler );
        return reqID;
    }

} // namespace ec2
