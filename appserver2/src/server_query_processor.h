/**********************************************************
* 29 jan 2014
* akolesnikov
***********************************************************/

#ifndef SERVER_QUERY_PROCESSOR_H
#define SERVER_QUERY_PROCESSOR_H

#include <QtConcurrent>
#include <QDateTime>

#include "cluster/cluster_manager.h"
#include "database/db_manager.h"
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
            void processUpdateAsync( const QnTransaction<QueryDataType>& tran, HandlerType handler )
        {
            //TODO/IMPL this method must be asynchronous
            ErrorCode errorCode = ErrorCode::ok;

            auto scopedGuardFunc = [&errorCode, &handler]( ServerQueryProcessor* ){
                QtConcurrent::run( std::bind( handler, errorCode ) );
            };
            std::unique_ptr<ServerQueryProcessor, decltype(scopedGuardFunc)> scopedGuard( this, scopedGuardFunc );

            errorCode = dbManager->executeTransaction( tran );
            if( errorCode != ErrorCode::ok )
                return;

            // saving transaction to the log
            errorCode = transactionLog->saveTransaction( tran );
            if( errorCode != ErrorCode::ok )
                return;

            // delivering transaction to remote peers
            QnTransactionMessageBus::instance()->sendTransaction(tran);

        }

        /*!
            \param handler Functor ( ErrorCode, OutputData )
            TODO let compiler guess template params
        */
        template<class InputData, class OutputData, class HandlerType>
            void processQueryAsync( ApiCommand::Value cmdCode, InputData input, HandlerType handler )
        {
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
            QtConcurrent::run( [input1, input2, handler]() {
                OutputData output;
                const ErrorCode errorCode = dbManager->doQuery( input1, input2, output );
                handler( errorCode, output );
            } );
        }
    };
}

#endif  //ABSTRACT_QUERY_PROCESSOR_H
