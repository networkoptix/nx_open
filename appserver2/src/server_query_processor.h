/**********************************************************
* 29 jan 2014
* akolesnikov
***********************************************************/

#ifndef SERVER_QUERY_PROCESSOR_H
#define SERVER_QUERY_PROCESSOR_H

#include <QtCore/QDateTime>

#include <utils/common/scoped_thread_rollback.h>
#include <utils/common/model_functions.h>
#include <utils/common/concurrent.h>

#include "ec2_thread_pool.h"
#include "database/db_manager.h"
#include "managers/aux_manager.h"
#include "transaction/transaction.h"
#include "transaction/transaction_log.h"
#include "transaction/transaction_message_bus.h"
#include <transaction/binary_transaction_serializer.h>


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


            QnDbManager::Locker locker(dbManager);
            if (ApiCommand::isPersistent(tran.command)) {
                locker.beginTran();
                tran.fillPersistentInfo();
            }

            auto SCOPED_GUARD_FUNC = [&errorCode, &handler]( ServerQueryProcessor* ){
                QnScopedThreadRollback ensureFreeThread( 1, Ec2ThreadPool::instance() );
                QnConcurrent::run( Ec2ThreadPool::instance(), std::bind( handler, errorCode ) );
            };
            std::unique_ptr<ServerQueryProcessor, decltype(SCOPED_GUARD_FUNC)>
                SCOPED_GUARD( this, SCOPED_GUARD_FUNC );

            errorCode = auxManager->executeTransaction(tran);
            if( errorCode != ErrorCode::ok )
                return;

            if( !tran.persistentInfo.isNull() )
            {
                const QByteArray& serializedTran = QnUbjsonTransactionSerializer::instance()->serializedTransaction( tran );
                errorCode = dbManager->executeTransactionNoLock( tran, serializedTran );
                if( errorCode != ErrorCode::ok )
                {
                    if( errorCode == ErrorCode::skipped )
                        errorCode = ErrorCode::ok;
                    return;
                }

                locker.commit();
            }

            // delivering transaction to remote peers
            if (!tran.isLocal)
                QnTransactionMessageBus::instance()->sendTransaction(tran);
        }

        template<class HandlerType>
        void processUpdateAsync( QnTransaction<ApiIdData>& tran, HandlerType handler )
        {
            switch (tran.command)
            {
            case ApiCommand::removeMediaServer:
                return processMultiUpdateAsync<ApiIdData, ApiIdData>(tran, handler, ApiCommand::removeCamera, dbManager->getNestedObjects(ApiObjectInfo(ApiObject_Server, tran.params.id)).toIdList(), true);
            case ApiCommand::removeUser:
                return processMultiUpdateAsync<ApiIdData, ApiIdData>(tran, handler, ApiCommand::removeLayout, dbManager->getNestedObjects(ApiObjectInfo(ApiObject_User, tran.params.id)).toIdList(), true);
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
            QList< QnTransaction<SubDataType> > processedTransactions;
            processedTransactions.reserve(static_cast<int>(nestedList.size()));

            bool processMultiTran = false;

            auto SCOPED_GUARD_FUNC = [&errorCode, &handler]( ServerQueryProcessor* ){
                QnScopedThreadRollback ensureFreeThread( 1, Ec2ThreadPool::instance() );
                QnConcurrent::run( Ec2ThreadPool::instance(), std::bind( handler, errorCode ) );
            };
            std::unique_ptr<ServerQueryProcessor, decltype(SCOPED_GUARD_FUNC)> SCOPED_GUARD( this, SCOPED_GUARD_FUNC );

            QnDbManager::Locker locker(dbManager);
            locker.beginTran();

            foreach(const SubDataType& data, nestedList)
            {
                QnTransaction<SubDataType> tran(command);
                tran.params = data;
                tran.fillPersistentInfo();
                tran.isLocal = multiTran.isLocal;

                errorCode = auxManager->executeTransaction(tran);
                if( errorCode != ErrorCode::ok )
                    return;

                QByteArray serializedTran = QnUbjsonTransactionSerializer::instance()->serializedTransaction(tran);
                errorCode = dbManager->executeTransactionNoLock( tran, serializedTran);
				if (errorCode == ErrorCode::skipped)
					continue;
                if( errorCode != ErrorCode::ok )
                    return;
                processedTransactions << tran;
            }
            
            // delete master object if need (server->cameras required to delete master object, layoutList->layout doesn't)
            if (isParentObjectTran) 
            {
                multiTran.fillPersistentInfo();

                errorCode = ErrorCode::ok;
                QByteArray serializedTran = QnUbjsonTransactionSerializer::instance()->serializedTransaction(multiTran);
                errorCode = dbManager->executeTransactionNoLock(multiTran, serializedTran);
                if( errorCode != ErrorCode::ok && errorCode != ErrorCode::skipped)
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
