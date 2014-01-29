/**********************************************************
* 29 jan 2014
* akolesnikov
***********************************************************/

#ifndef SERVER_QUERY_PROCESSOR_H
#define SERVER_QUERY_PROCESSOR_H

#include <QtConcurrent>

#include "cluster/cluster_manager.h"
#include "database/db_manager.h"
#include "transaction/transaction.h"
#include "transaction/transaction_log.h"


namespace ec2
{
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
            ErrorCode errorCode = dbManager->executeTransaction( tran );
            if( errorCode != ErrorCode::ok )
            {
                QtConcurrent::run( std::bind( handler, errorCode ) );
                return;
            }

            // saving transaction to the log
            errorCode = transactionLog->saveTransaction( tran );
            if( errorCode != ErrorCode::ok )
            {
                QtConcurrent::run( std::bind( handler, errorCode ) );
                return;
            }

            // delivering transaction to remote peers
            clusterManager->distributeAsync( tran );
            QtConcurrent::run( std::bind( handler, errorCode ) );
        }

        /*!
            \param handler Functor ( ErrorCode, OutputData )
            TODO let compiler guess template params
        */
        template<class InputData, class OutputData, class HandlerType>
            void processQueryAsync( ApiCommand /*cmd*/, InputData input, HandlerType handler )
        {
            auto func = [input, handler]() {
                OutputData output;
                ErrorCode errorCode = dbManager->doQuery( input, output );
                handler( errorCode, output );
            };
            QtConcurrent::run( func );
        }
    };
}

#endif  //ABSTRACT_QUERY_PROCESSOR_H
