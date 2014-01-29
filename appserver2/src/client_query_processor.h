/**********************************************************
* 29 jan 2014
* akolesnikov
***********************************************************/

#ifndef CLIENT_QUERY_PROCESSOR_H
#define CLIENT_QUERY_PROCESSOR_H

#include <QtConcurrent>

#include "cluster/cluster_manager.h"
#include "database/db_manager.h"
#include "transaction/transaction.h"
#include "transaction/transaction_log.h"


namespace ec2
{
    class ClientQueryProcessor
    {
    public:
        virtual ~ClientQueryProcessor() {}

        //!Asynchronously executes update query
        /*!
            \param handler Functor ( ErrorCode )
        */
        template<class QueryDataType, class HandlerType>
            void processUpdateAsync( const QnTransaction<QueryDataType>& /*tran*/, HandlerType handler )
        {
            //TODO/IMPL
            QtConcurrent::run( std::bind( handler, ErrorCode::failure ) );
        }

        /*!
            \param handler Functor ( ErrorCode, OutputData )
            TODO let compiler guess template params
        */
        template<class InputData, class OutputData, class HandlerType>
            void processQueryAsync( ApiCommand /*cmd*/, InputData /*input*/, HandlerType handler )
        {
            //TODO/IMPL
            QtConcurrent::run( std::bind( handler, ErrorCode::failure, OutputData() ) );
        }
    };
}

#endif  //CLIENT_QUERY_PROCESSOR_H
