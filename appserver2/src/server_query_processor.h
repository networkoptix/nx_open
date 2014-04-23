/**********************************************************
* 29 jan 2014
* akolesnikov
***********************************************************/

#ifndef SERVER_QUERY_PROCESSOR_H
#define SERVER_QUERY_PROCESSOR_H

#include <QtConcurrent>
#include <QDateTime>

#include <utils/common/scoped_thread_rollback.h>

#include "cluster/cluster_manager.h"
#include "database/db_manager.h"
#include "managers/aux_manager.h"
#include "transaction/transaction.h"
#include "transaction/transaction_log.h"


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

            auto scopedGuardFunc = [&errorCode, &handler]( ServerQueryProcessor* ){
                QnScopedThreadRollback ensureFreeThread(1);
                QtConcurrent::run( std::bind( handler, errorCode ) );
            };
            std::unique_ptr<ServerQueryProcessor, decltype(scopedGuardFunc)> scopedGuard( this, scopedGuardFunc );

            QByteArray serializedTran;
            QnOutputBinaryStream<QByteArray> stream( &serializedTran );
            QnBinary::serialize( tran, &stream );

            errorCode = auxManager->executeTransaction(tran);
            if( errorCode != ErrorCode::ok )
                return;

            if (tran.persistent) {
                errorCode = dbManager->executeTransaction( tran, serializedTran);
                if( errorCode != ErrorCode::ok )
                    return;
            }

            // delivering transaction to remote peers
            if (!tran.localTransaction)
                QnTransactionMessageBus::instance()->sendTransaction(tran, serializedTran);
        }

        template<class HandlerType>
        void processUpdateAsync(QnTransaction<ApiLicenseList>& tran, HandlerType handler )
        {
            Q_ASSERT(tran.command == ApiCommand::addLicenses);
            return processMultiUpdateAsync<ApiLicenseList, ApiLicense>(tran, handler, ApiCommand::addLicense);
        }

        template<class HandlerType>
        void processUpdateAsync(QnTransaction<ApiLayoutList>& tran, HandlerType handler )
        {
            Q_ASSERT(tran.command == ApiCommand::saveLayouts);
            return processMultiUpdateAsync<ApiLayoutList, ApiLayoutData>(tran, handler, ApiCommand::saveLayout);
        }

        template<class HandlerType>
        void processUpdateAsync(QnTransaction<ApiCameraList>& tran, HandlerType handler )
        {
            Q_ASSERT(tran.command == ApiCommand::saveCameras);
            return processMultiUpdateAsync<ApiCameraList, ApiCamera>(tran, handler, ApiCommand::saveCamera);
        }

        template<class QueryDataType, class subDataType, class HandlerType>
        void processMultiUpdateAsync(QnTransaction<QueryDataType>& multiTran, HandlerType handler, ApiCommand::Value command)
        {
            ErrorCode errorCode = ErrorCode::ok;
            QList<QPair<QnTransaction<subDataType>, QByteArray>> processedTran;
            
            if (multiTran.persistent)
                dbManager->beginTran();

            foreach(const subDataType& data, multiTran.params.data)
            {
                QnTransaction<subDataType> tran(command, multiTran.persistent);
                tran.params = data;
                tran.fillSequence();
                tran.localTransaction = multiTran.localTransaction;

                auto scopedGuardFunc = [&errorCode, &handler]( ServerQueryProcessor* ){
                    QnScopedThreadRollback ensureFreeThread(1);
                    QtConcurrent::run( std::bind( handler, errorCode ) );
                };
                std::unique_ptr<ServerQueryProcessor, decltype(scopedGuardFunc)> scopedGuard( this, scopedGuardFunc );

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
                    if( errorCode != ErrorCode::ok ) {
                        dbManager->rollback();
                        return;
                    }
                }
                processedTran << QPair<QnTransaction<subDataType>, QByteArray>(tran, serializedTran);
            }

            if (multiTran.persistent)
                dbManager->commit();

            foreach(const auto& tranData, processedTran)
            {
                // delivering transaction to remote peers
                if (!tranData.first.localTransaction)
                    QnTransactionMessageBus::instance()->sendTransaction(tranData.first, tranData.second);
            }

        }


        template<class T> 
        bool processIncomingTransaction( const QnTransaction<T>& tran, const QByteArray& serializedTran ) 
        {
            if (tran.persistent)
            {
                ErrorCode errorCode = dbManager->executeTransaction( tran, serializedTran );
                if( errorCode != ErrorCode::ok )
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
