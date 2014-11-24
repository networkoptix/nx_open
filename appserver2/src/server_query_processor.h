/**********************************************************
* 29 jan 2014
* akolesnikov
***********************************************************/

#ifndef SERVER_QUERY_PROCESSOR_H
#define SERVER_QUERY_PROCESSOR_H

#include <QtCore/QDateTime>
#include <QtCore/QDebug>

#include <utils/common/scoped_thread_rollback.h>
#include <utils/common/model_functions.h>
#include <utils/common/concurrent.h>

#include "ec2_thread_pool.h"
#include "database/db_manager.h"
#include "transaction/transaction.h"
#include "transaction/transaction_log.h"
#include "transaction/transaction_message_bus.h"
#include <transaction/binary_transaction_serializer.h>


namespace ec2
{
    struct SendTransactionFunction
    {
        template<class T> 
        void operator()(const QnTransaction<T> &tran) const {
            qnTransactionBus->sendTransaction( tran, tran.isLocal ? qnTransactionBus->aliveClientPeers().keys().toSet() : QnPeerSet());
        }
    };

    class CommonRequestsProcessor
    {
    public:
        static ErrorCode getCurrentTime( std::nullptr_t, qint64* curTime )
        {
            *curTime = QDateTime::currentDateTime().toMSecsSinceEpoch();
            return ErrorCode::ok;
        }
    };


    class ServerQueryProcessor
    {
    public:
        virtual ~ServerQueryProcessor() {}

        template<class QueryDataType, class HandlerType>
        void processUpdateAsync( QnTransaction<QueryDataType>& tran, HandlerType handler, void* /*dummy*/ = 0 )
        {
            using namespace std::placeholders;
            doAsyncExecuteTranCall(
                tran,
                handler,
                [this]( QnTransaction<QueryDataType>& tran, std::list<std::function<void()>>* const transactionsToSend ) -> ErrorCode {
                    return processUpdateSync( tran, transactionsToSend );
                } );
        }

        /*!
            \param syncFunction ErrorCode( QnTransaction<QueryDataType>& , std::list<std::function<void()>>* )
        */
        template<class QueryDataType, class CompletionHandlerType, class SyncFunctionType>
        void doAsyncExecuteTranCall(
                QnTransaction<QueryDataType>& tran,
                CompletionHandlerType completionHandler,
                SyncFunctionType syncFunction )
        {
            ErrorCode errorCode = ErrorCode::ok;
            auto SCOPED_GUARD_FUNC = [&errorCode, &completionHandler]( ServerQueryProcessor* ){
                QnScopedThreadRollback ensureFreeThread( 1, Ec2ThreadPool::instance() );
                QnConcurrent::run( Ec2ThreadPool::instance(), std::bind( completionHandler, errorCode ) );
            };
            std::unique_ptr<ServerQueryProcessor, decltype(SCOPED_GUARD_FUNC)> SCOPED_GUARD( this, SCOPED_GUARD_FUNC );

            QMutexLocker lock(&m_updateDataMutex);

            //starting transaction
            std::unique_ptr<QnDbManager::Locker> dbTran;
            std::list<std::function<void()>> transactionsToSend;

            if( ApiCommand::isPersistent(tran.command) )
            {
                dbTran.reset(new QnDbManager::Locker(dbManager));
                errorCode = syncFunction( tran, &transactionsToSend );
                if( errorCode != ErrorCode::ok )
                    return;
                if (!dbTran->commit())
                    return;
            }
            else 
            {
                transactionsToSend.push_back( std::bind(SendTransactionFunction(), tran ) );
            }

            //sending transactions
            for( auto& sendCommand: transactionsToSend )
                sendCommand();
                
            //handler is invoked asynchronously
        }

        template<class HandlerType>
        void removeResourceAsync(
            QnTransaction<ApiIdData>& tran,
            ApiOjectType resourceType,
            HandlerType handler )
        {
            using namespace std::placeholders;
            doAsyncExecuteTranCall(
                tran,
                handler,
                std::bind( &ServerQueryProcessor::removeResourceSync, this, _1, resourceType, _2 ) );
        }
            
        ErrorCode removeResourceSync(
            QnTransaction<ApiIdData>& tran,
            ApiOjectType resourceType,
            std::list<std::function<void()>>* const transactionsToSend )
        {
            ErrorCode errorCode = ErrorCode::ok;

            errorCode = processMultiUpdateSync(
                ApiCommand::removeResource,
                tran.isLocal,
                dbManager->getNestedObjects(ApiObjectInfo(resourceType, tran.params.id)).toIdList(),
                transactionsToSend );
            if( errorCode != ErrorCode::ok )
                return errorCode;

            return processUpdateSync( tran, transactionsToSend, 0 );
        }

        template<class QueryDataType>
        ErrorCode processUpdateSync(
            QnTransaction<QueryDataType>& tran,
            std::list<std::function<void()>>* const transactionsToSend,
            int /*dummy*/ = 0 )
        {
            Q_ASSERT( ApiCommand::isPersistent(tran.command) );

            transactionLog->fillPersistentInfo(tran);
            QByteArray serializedTran = QnUbjsonTransactionSerializer::instance()->serializedTransaction(tran);
            ErrorCode errorCode = dbManager->executeTransactionNoLock( tran, serializedTran );
            assert(errorCode != ErrorCode::containsBecauseSequence && errorCode != ErrorCode::containsBecauseTimestamp);
            if (errorCode != ErrorCode::ok)
                return errorCode;

            transactionsToSend->push_back( std::bind(SendTransactionFunction(), tran ) );

            return errorCode;
        }

        ErrorCode processUpdateSync(
            QnTransaction<ApiIdData>& tran,
            std::list<std::function<void()>>* const transactionsToSend )
        {
            switch (tran.command)
            {
            case ApiCommand::removeMediaServer:
                return removeResourceSync( tran, ApiObject_Server, transactionsToSend );
            case ApiCommand::removeUser:
                return removeResourceSync( tran, ApiObject_User, transactionsToSend );
            case ApiCommand::removeResource:
            {
                QnTransaction<ApiIdData> updatedTran = tran;
                switch(dbManager->getObjectType(tran.params.id))
                {
                case ApiObject_Server:
                    updatedTran.command = ApiCommand::removeMediaServer;
                    break;
                case ApiObject_Camera:
                    updatedTran.command = ApiCommand::removeCamera;
                    break;
                case ApiObject_Storage:
                    updatedTran.command = ApiCommand::removeStorage;
                    break;
                case ApiObject_User:
                    updatedTran.command = ApiCommand::removeUser;
                    break;
                case ApiObject_Layout:
                    updatedTran.command = ApiCommand::removeLayout;
                    break;
                case ApiObject_Videowall:
                    updatedTran.command = ApiCommand::removeVideowall;
                    break;
                case ApiObject_BusinessRule:
                    updatedTran.command = ApiCommand::removeBusinessRule;
                    break;
                default:
                    return processUpdateSync( tran, transactionsToSend, 0 );
                }
                return processUpdateSync( updatedTran, transactionsToSend );    //calling recursively
            }
            default:
                return processUpdateSync( tran, transactionsToSend, 0 );
            }
        }

        template<class SubDataType>
        ErrorCode processMultiUpdateSync(
            ApiCommand::Value command,
            bool isLocal,
            const std::vector<SubDataType>& nestedList,
            std::list<std::function<void()>>* const transactionsToSend )
        {
            for(const SubDataType& data: nestedList)
            {
                QnTransaction<SubDataType> subTran(command, data);
                subTran.isLocal = isLocal;
                ErrorCode errorCode = processUpdateSync( subTran, transactionsToSend );
                if (errorCode != ErrorCode::ok)
                    return errorCode;
            }

            return ErrorCode::ok;
        }

        template<class QueryDataType, class SubDataType, class HandlerType>
        void processMultiUpdateAsync(
            QnTransaction<QueryDataType>& multiTran,
            HandlerType handler,
            ApiCommand::Value subCommand )
        {
            using namespace std::placeholders;
            doAsyncExecuteTranCall(
                multiTran,
                handler,
                [this, subCommand]( QnTransaction<QueryDataType>& multiTran, std::list<std::function<void()>>* const transactionsToSend ) -> ErrorCode {
                    return processMultiUpdateSync( subCommand, multiTran.isLocal, multiTran.params, transactionsToSend );
                } );
        }

        template<class HandlerType>
        void processUpdateAsync(QnTransaction<ApiIdDataList>& tran, HandlerType handler )
        {
            if (tran.command == ApiCommand::removeStorages)             {
                return processMultiUpdateAsync<ApiIdDataList, ApiIdData>(tran, handler, ApiCommand::removeStorage);
            }
            else if (tran.command == ApiCommand::removeResources) 
            {
                return processMultiUpdateAsync<ApiIdDataList, ApiIdData>(tran, handler, ApiCommand::removeResource);
            }
            else {
                Q_ASSERT_X(0, "Not implemented", Q_FUNC_INFO);
            }
        }


        template<class HandlerType>
        void processUpdateAsync( QnTransaction<ApiIdData>& tran, HandlerType handler )
        {
            //TODO #ak there is processUpdateSync with same switch. Remove switch from here!

            switch (tran.command)
            {
            case ApiCommand::removeMediaServer:
                return removeResourceAsync( tran, ApiObject_Server, handler );
            case ApiCommand::removeUser:
                return removeResourceAsync( tran, ApiObject_User, handler );
            case ApiCommand::removeResource:
            {
                QnTransaction<ApiIdData> updatedTran = tran;
                switch(dbManager->getObjectType(tran.params.id))
                {
                case ApiObject_Server:
                    updatedTran.command = ApiCommand::removeMediaServer;
                    break;
                case ApiObject_Camera:
                    updatedTran.command = ApiCommand::removeCamera;
                    break;
                case ApiObject_Storage:
                    updatedTran.command = ApiCommand::removeStorage;
                    break;
                case ApiObject_User:
                    updatedTran.command = ApiCommand::removeUser;
                    break;
                case ApiObject_Layout:
                    updatedTran.command = ApiCommand::removeLayout;
                    break;
                case ApiObject_Videowall:
                    updatedTran.command = ApiCommand::removeVideowall;
                    break;
                case ApiObject_BusinessRule:
                    updatedTran.command = ApiCommand::removeBusinessRule;
                    break;
                default:
                    return processUpdateAsync(tran, handler, 0); // call default handler
                }
                return processUpdateAsync(updatedTran, handler);
            }
            default:
                return processUpdateAsync(tran, handler, 0); // call default handler
            }
        }

        template<class HandlerType>
        void processUpdateAsync(QnTransaction<ApiLicenseDataList>& tran, HandlerType handler )
        {
            Q_ASSERT(tran.command == ApiCommand::addLicenses);
            return processMultiUpdateAsync<ApiLicenseDataList, ApiLicenseData>(tran, handler, ApiCommand::addLicense);
        }

        template<class HandlerType>
        void processUpdateAsync(QnTransaction<ApiLayoutDataList>& tran, HandlerType handler )
        {
            Q_ASSERT(tran.command == ApiCommand::saveLayouts);
            return processMultiUpdateAsync<ApiLayoutDataList, ApiLayoutData>(tran, handler, ApiCommand::saveLayout);
        }

        template<class HandlerType>
        void processUpdateAsync(QnTransaction<ApiCameraDataList>& tran, HandlerType handler )
        {
            Q_ASSERT(tran.command == ApiCommand::saveCameras);
            return processMultiUpdateAsync<ApiCameraDataList, ApiCameraData>(tran, handler, ApiCommand::saveCamera);
        }

        template<class HandlerType>
        void processUpdateAsync(QnTransaction<ApiStorageDataList>& tran, HandlerType handler )
        {
            Q_ASSERT(tran.command == ApiCommand::saveStorages);
            return processMultiUpdateAsync<ApiStorageDataList, ApiStorageData>(tran, handler, ApiCommand::saveStorage);
        }

        template<class HandlerType>
        void processUpdateAsync(QnTransaction<ApiCameraAttributesDataList>& tran, HandlerType handler )
        {
            Q_ASSERT(tran.command == ApiCommand::saveCameraUserAttributesList);
            return processMultiUpdateAsync<ApiCameraAttributesDataList, ApiCameraAttributesData>(tran, handler, ApiCommand::saveCameraUserAttributes);
        }

        template<class HandlerType>
        void processUpdateAsync(QnTransaction<ApiMediaServerUserAttributesDataList>& tran, HandlerType handler )
        {
            Q_ASSERT(tran.command == ApiCommand::saveServerUserAttributesList);
            return processMultiUpdateAsync<ApiMediaServerUserAttributesDataList, ApiMediaServerUserAttributesData>(tran, handler, ApiCommand::saveServerUserAttributes);
        }

        template<class HandlerType>
        void processUpdateAsync(QnTransaction<ApiResourceParamWithRefDataList>& tran, HandlerType handler )
        {
            if(tran.command == ApiCommand::setResourceParams)
                return processMultiUpdateAsync<ApiResourceParamWithRefDataList, ApiResourceParamWithRefData>(tran, handler, ApiCommand::setResourceParam);
            else if(tran.command == ApiCommand::removeResourceParams)
                return processMultiUpdateAsync<ApiResourceParamWithRefDataList, ApiResourceParamWithRefData>(tran, handler, ApiCommand::removeResourceParam);
            else
                Q_ASSERT_X(0, "Not implemented!", Q_FUNC_INFO);
        }

        /*!
            \param handler Functor ( ErrorCode, OutputData )
            TODO let compiler guess template params
        */
        template<class InputData, class OutputData, class HandlerType>
            void processQueryAsync( ApiCommand::Value /*cmdCode*/, InputData input, HandlerType handler )
        {
            QnScopedThreadRollback ensureFreeThread( 1, Ec2ThreadPool::instance() );
            QnConcurrent::run( Ec2ThreadPool::instance(), [input, handler]() {
                OutputData output;
                const ErrorCode errorCode = dbManager->doQuery( input, output );
                handler( errorCode, output );
            } );
        }

        /*!
            \param handler Functor ( ErrorCode, OutputData )
            TODO let compiler guess template params
        */
        template<class OutputData, class InputParamType1, class InputParamType2, class HandlerType>
            void processQueryAsync( ApiCommand::Value /*cmdCode*/, InputParamType1 input1, InputParamType2 input2, HandlerType handler )
        {
            QnScopedThreadRollback ensureFreeThread( 1, Ec2ThreadPool::instance() );
            QnConcurrent::run( Ec2ThreadPool::instance(), [input1, input2, handler]() {
                OutputData output;
                const ErrorCode errorCode = dbManager->doQuery( input1, input2, output );
                handler( errorCode, output );
            } );
        }
        private:
            static QMutex m_updateDataMutex;
    };
}

#endif  //ABSTRACT_QUERY_PROCESSOR_H
