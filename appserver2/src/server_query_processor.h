/**********************************************************
* 29 jan 2014
* akolesnikov
***********************************************************/

#ifndef SERVER_QUERY_PROCESSOR_H
#define SERVER_QUERY_PROCESSOR_H

#include <QtCore/QDateTime>
#include <QtConcurrent>

#include <utils/common/scoped_thread_rollback.h>
#include <utils/common/model_functions.h>

#include "database/db_manager.h"
#include "managers/aux_manager.h"
#include "transaction/transaction.h"
#include "transaction/transaction_log.h"
#include "transaction/transaction_message_bus.h"


namespace ec2
{
    class CommonRequestsProcessor
    {
    public:
        static ErrorCode getCurrentTime( nullptr_t, qint64* curTime )
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
            void processUpdateAsync(QnTransaction<QueryDataType>& tran, HandlerType handler )
        {
            //TODO/IMPL this method must be asynchronous
            ErrorCode errorCode = ErrorCode::ok;

            if (!tran.id.sequence)
                tran.fillSequence();

            auto SCOPED_GUARD_FUNC = [&errorCode, &handler]( ServerQueryProcessor* ){
                QnScopedThreadRollback ensureFreeThread(1);
                QtConcurrent::run( std::bind( handler, errorCode ) );
            };
            std::unique_ptr<ServerQueryProcessor, decltype(SCOPED_GUARD_FUNC)> SCOPED_GUARD( this, SCOPED_GUARD_FUNC );

            QByteArray serializedTran;
            QnOutputBinaryStream<QByteArray> stream( &serializedTran );
            QnBinary::serialize( tran, &stream );

            errorCode = auxManager->executeTransaction(tran);
            if( errorCode != ErrorCode::ok )
                return;

            if (tran.persistent) {
                errorCode = dbManager->executeTransaction( tran, serializedTran);
                if( errorCode != ErrorCode::ok ) {
                    if (errorCode == ErrorCode::skipped)
                        errorCode = ErrorCode::ok;
                    return;
                }
            }

            // delivering transaction to remote peers
            if (!tran.localTransaction)
                QnTransactionMessageBus::instance()->sendTransaction(tran, serializedTran);
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
            ErrorCode errorCode = ErrorCode::ok;
            QList<QPair<QnTransaction<SubDataType>, QByteArray>> processedTran;
            
            if (multiTran.persistent)
                dbManager->beginTran();

            foreach(const SubDataType& data, multiTran.params)
            {
                QnTransaction<SubDataType> tran(command, multiTran.persistent);
                tran.params = data;
                tran.fillSequence();
                tran.localTransaction = multiTran.localTransaction;

                auto SCOPED_GUARD_FUNC = [&errorCode, &handler]( ServerQueryProcessor* ){
                    QnScopedThreadRollback ensureFreeThread(1);
                    QtConcurrent::run( std::bind( handler, errorCode ) );
                };
                std::unique_ptr<ServerQueryProcessor, decltype(SCOPED_GUARD_FUNC)> SCOPED_GUARD( this, SCOPED_GUARD_FUNC );

                QByteArray serializedTran;
                QnOutputBinaryStream<QByteArray> stream( &serializedTran );
                QnBinary::serialize( tran, &stream );

                errorCode = auxManager->executeTransaction(tran);
                if( errorCode != ErrorCode::ok ) {
                    if (multiTran.persistent)
                        dbManager->rollback();
                    return;
                }

                if (tran.persistent) {
                    errorCode = dbManager->executeNestedTransaction( tran, serializedTran);
					if (errorCode == ErrorCode::skipped)
						continue;
                    if( errorCode != ErrorCode::ok ) {
                        dbManager->rollback();
                        return;
                    }
                }
                processedTran << QPair<QnTransaction<SubDataType>, QByteArray>(tran, serializedTran);
            }

            if (multiTran.persistent)
                dbManager->commit();

            foreach(const auto& tranData, processedTran)
            {
                // delivering transaction to remote peers
                if (!tranData.first.localTransaction)
                    QnTransactionMessageBus::instance()->sendTransaction(tranData.first, tranData.second);
            }
            errorCode = ErrorCode::ok;
        }


        template<class T> 
        bool processIncomingTransaction( const QnTransaction<T>& tran, const QByteArray& serializedTran ) 
        {
            if (tran.persistent)
            {
                ErrorCode errorCode = dbManager->executeTransaction( tran, serializedTran );
                if( errorCode != ErrorCode::ok && errorCode != ErrorCode::skipped)
                    return false;
            }
            return true;
        }


        /*!
            \param handler Functor ( ErrorCode, OutputData )
            TODO let compiler guess template params
        */
        template<class InputData, class OutputData, class HandlerType>
            void processQueryAsync( ApiCommand::Value /*cmdCode*/, InputData input, HandlerType handler )
        {
            QnScopedThreadRollback ensureFreeThread(1);
            QtConcurrent::run( [input, handler]() {
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
            QnScopedThreadRollback ensureFreeThread(1);
            QtConcurrent::run( [input1, input2, handler]() {
                OutputData output;
                const ErrorCode errorCode = dbManager->doQuery( input1, input2, output );
                handler( errorCode, output );
            } );
        }
    };
}

#endif  //ABSTRACT_QUERY_PROCESSOR_H
