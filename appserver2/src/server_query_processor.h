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


#define REFACTOR

namespace ec2
{
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

#ifndef REFACTOR
        //!Asynchronously executes update query
        /*!
            \param handler Functor ( ErrorCode )
        */
        template<class QueryDataType, class HandlerType>
            void processUpdateAsync( QnTransaction<QueryDataType>& tran, HandlerType handler, void* /*dummy*/ = 0 )
        {
            QMutexLocker lock(&m_updateDataMutex);

            //TODO #ak this method must be asynchronous
            ErrorCode errorCode = ErrorCode::ok;


            std::unique_ptr<QnDbManager::Locker> locker;
            if (ApiCommand::isPersistent(tran.command)) {
                locker.reset(new QnDbManager::Locker(dbManager));
                transactionLog->fillPersistentInfo(tran);
            }

            auto SCOPED_GUARD_FUNC = [&errorCode, &handler]( ServerQueryProcessor* ){
                QnScopedThreadRollback ensureFreeThread( 1, Ec2ThreadPool::instance() );
                QnConcurrent::run( Ec2ThreadPool::instance(), std::bind( handler, errorCode ) );
            };
            std::unique_ptr<ServerQueryProcessor, decltype(SCOPED_GUARD_FUNC)>
                SCOPED_GUARD( this, SCOPED_GUARD_FUNC );

            if( !tran.persistentInfo.isNull() )
            {
                const QByteArray& serializedTran = QnUbjsonTransactionSerializer::instance()->serializedTransaction( tran );
                errorCode = dbManager->executeTransactionNoLock( tran, serializedTran );
                assert(errorCode != ErrorCode::containsBecauseSequence && errorCode != ErrorCode::containsBecauseTimestamp);
                if( errorCode != ErrorCode::ok )
                    return;

                locker->commit();
            }

            // delivering transaction to remote peers
            if (!tran.isLocal)
                QnTransactionMessageBus::instance()->sendTransaction(tran);
        }
#endif


#ifdef REFACTOR
        /////////////////////////////
        /////// new functions
        /////////////////////////////

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
                //TODO #ak commit MUST return error code !!! it can really fail
                dbTran->commit();
            }
            else if( !tran.isLocal )
            {
                //TODO #ak tran is copied here. Probably it is possible to do move here or just take reference
                transactionsToSend.push_back( [tran](){ QnTransactionMessageBus::instance()->sendTransaction( tran ); } );
                //TODO #ak why std::bind does not work here?
                //transactionsToSend.push_back( std::bind(
                //    &QnTransactionMessageBus::sendTransaction<QueryDataType>,
                //    QnTransactionMessageBus::instance(),
                //    tran ) );
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

            if( !tran.isLocal )
                transactionsToSend->push_back( [tran](){ QnTransactionMessageBus::instance()->sendTransaction( tran ); } );

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
            foreach(const SubDataType& data, nestedList)
            {
                QnTransaction<SubDataType> subTran(command);
                subTran.params = data;
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
#if 1
            using namespace std::placeholders;
            doAsyncExecuteTranCall(
                multiTran,
                handler,
                [this, subCommand]( QnTransaction<QueryDataType>& multiTran, std::list<std::function<void()>>* const transactionsToSend ) -> ErrorCode {
                    return processMultiUpdateSync( subCommand, multiTran.isLocal, multiTran.params, transactionsToSend );
                } );
#else
            QMutexLocker lock(&m_updateDataMutex);

            Q_ASSERT(ApiCommand::isPersistent(multiTran.command));

            auto SCOPED_GUARD_FUNC = [&errorCode, &handler]( ServerQueryProcessor* ){
                QnScopedThreadRollback ensureFreeThread( 1, Ec2ThreadPool::instance() );
                QnConcurrent::run( Ec2ThreadPool::instance(), std::bind( handler, errorCode ) );
            };
            std::unique_ptr<ServerQueryProcessor, decltype(SCOPED_GUARD_FUNC)> SCOPED_GUARD( this, SCOPED_GUARD_FUNC );

            QnDbManager::Locker dbTran(dbManager);

            std::list<std::function<void()>> transactionsToSend;
            ErrorCode errorCode = processMultiUpdateSync( subCommand, multiTran.params, &transactionsToSend );
            if( errorCode != ErrorCode::ok )
                return;

            //TODO #ak commit MUST return error code !!! it can really fail
            dbTran.commit();

            for( auto& sendCommand: transactionsToSend )
                sendCommand();

            //async completion handler is invoked on return
#endif
        }



#if 0
        // old function processMultiUpdateAsync can be modified to look like this: but it is not needed anymore
        template<class QueryDataType, class SubDataType, class HandlerType>
        void processMultiUpdateAsync(
            QnTransaction<QueryDataType>& multiTran,
            HandlerType handler,
            ApiCommand::Value command,
            const std::vector<SubDataType>& nestedList,
            bool isParentObjectTran )
        {
            QMutexLocker lock(&m_updateDataMutex);

            Q_ASSERT(ApiCommand::isPersistent(multiTran.command));

            std::list<std::function<void()>> transactionsToSend;
            ErrorCode errorCode = ErrorCode::ok;

            bool processMultiTran = false;

            auto SCOPED_GUARD_FUNC = [&errorCode, &handler]( ServerQueryProcessor* ){
                QnScopedThreadRollback ensureFreeThread( 1, Ec2ThreadPool::instance() );
                QnConcurrent::run( Ec2ThreadPool::instance(), std::bind( handler, errorCode ) );
            };
            std::unique_ptr<ServerQueryProcessor, decltype(SCOPED_GUARD_FUNC)> SCOPED_GUARD( this, SCOPED_GUARD_FUNC );

            QnDbManager::Locker locker(dbManager);

            errorCode = processMultiUpdateSync( command, nestedList, &transactionsToSend );
            if (errorCode != ErrorCode::ok)
                return;
            
            // delete master object if need (server->cameras required to delete master object, layoutList->layout doesn't)
            if( isParentObjectTran )
            {
                errorCode = processUpdateSync( multiTran, &transactionsToSend );
                if( errorCode != ErrorCode::ok )
                    return;
                processMultiTran = true;
            }

            locker.commit();

            for( auto& sendCommand: transactionsToSend )
                sendCommand();
        }
#endif



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



        /////////////////////////////
        /////// new functions (end)
        /////////////////////////////

#endif  //REFACTOR



        template<class HandlerType>
        void processUpdateAsync( QnTransaction<ApiIdData>& tran, HandlerType handler )
        {
            //TODO #ak there is processUpdateSync with same switch. Remove switch from here!

            switch (tran.command)
            {
            case ApiCommand::removeMediaServer:
#ifdef REFACTOR
                return removeResourceAsync( tran, ApiObject_Server, handler );
#else
                return processMultiUpdateAsync<ApiIdData, ApiIdData>(tran, handler, ApiCommand::removeCamera, dbManager->getNestedObjects(ApiObjectInfo(ApiObject_Server, tran.params.id)).toIdList(), true);
#endif
            case ApiCommand::removeUser:
#ifdef REFACTOR
                return removeResourceAsync( tran, ApiObject_User, handler );
#else
                return processMultiUpdateAsync<ApiIdData, ApiIdData>(tran, handler, ApiCommand::removeLayout, dbManager->getNestedObjects(ApiObjectInfo(ApiObject_User, tran.params.id)).toIdList(), true);
#endif
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

#ifndef REFACTOR
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
#endif

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

#ifndef REFACTOR
        template<class QueryDataType, class SubDataType, class HandlerType>
        void processMultiUpdateAsync(QnTransaction<QueryDataType>& multiTran, HandlerType handler, ApiCommand::Value command)
        {
            return processMultiUpdateAsync(multiTran, handler, command, multiTran.params, false);
        }


        template<class QueryDataType, class SubDataType, class HandlerType>
        void processMultiUpdateAsync(QnTransaction<QueryDataType>& multiTran, HandlerType handler, ApiCommand::Value command, const std::vector<SubDataType>& nestedList, bool isParentObjectTran)
        {
            QMutexLocker lock(&m_updateDataMutex);

            Q_ASSERT(ApiCommand::isPersistent(multiTran.command));

            ErrorCode errorCode = ErrorCode::ok;
            std::vector< QnTransaction<SubDataType> > processedTransactions;
            processedTransactions.reserve(static_cast<int>(nestedList.size()));

            bool processMultiTran = false;

            auto SCOPED_GUARD_FUNC = [&errorCode, &handler]( ServerQueryProcessor* ){
                QnScopedThreadRollback ensureFreeThread( 1, Ec2ThreadPool::instance() );
                QnConcurrent::run( Ec2ThreadPool::instance(), std::bind( handler, errorCode ) );
            };
            std::unique_ptr<ServerQueryProcessor, decltype(SCOPED_GUARD_FUNC)> SCOPED_GUARD( this, SCOPED_GUARD_FUNC );

            QnDbManager::Locker locker(dbManager);

            foreach(const SubDataType& data, nestedList)
            {
                QnTransaction<SubDataType> tran(command);
                tran.params = data;
                tran.isLocal = multiTran.isLocal;
                transactionLog->fillPersistentInfo(tran);

                QByteArray serializedTran = QnUbjsonTransactionSerializer::instance()->serializedTransaction(tran);
                errorCode = dbManager->executeTransactionNoLock( tran, serializedTran);
                assert(errorCode != ErrorCode::containsBecauseSequence && errorCode != ErrorCode::containsBecauseTimestamp);
                if (errorCode != ErrorCode::ok)
                    return;
                processedTransactions.push_back( std::move(tran) );
            }
            
            // delete master object if need (server->cameras required to delete master object, layoutList->layout doesn't)
            if (isParentObjectTran) 
            {
                transactionLog->fillPersistentInfo(multiTran);

                errorCode = ErrorCode::ok;
                QByteArray serializedTran = QnUbjsonTransactionSerializer::instance()->serializedTransaction(multiTran);
                errorCode = dbManager->executeTransactionNoLock(multiTran, serializedTran);
                assert(errorCode != ErrorCode::containsBecauseSequence && errorCode != ErrorCode::containsBecauseTimestamp);
                if (errorCode != ErrorCode::ok)
                    return;
                processMultiTran = (errorCode == ErrorCode::ok);
            }


            locker.commit();

            foreach(const QnTransaction<SubDataType>& transaction, processedTransactions)
            {
                // delivering transaction to remote peers
                if (!transaction.isLocal)
                    QnTransactionMessageBus::instance()->sendTransaction(transaction);
            }
            if (processMultiTran)
                QnTransactionMessageBus::instance()->sendTransaction(multiTran);

            errorCode = ErrorCode::ok;
        }

#endif

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
