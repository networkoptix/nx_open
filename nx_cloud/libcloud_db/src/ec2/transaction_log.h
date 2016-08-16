/**********************************************************
* Aug 16, 2016
* a.kolesnikov
***********************************************************/

#pragma once

#include <utils/db/async_sql_query_executor.h>

#include "transaction_transport_header.h"


namespace nx {
namespace cdb {
namespace ec2 {

/**  */
class TransactionLog
{
public:
    template<typename TransactionDataType>
    bool checkIfNeededAndSaveToLog(
        QSqlDatabase* /*connection*/,
        const nx::String& /*systemId*/,
        ::ec2::QnTransaction<TransactionDataType> /*transaction*/)
    {
        //TODO
        return false;
    }
};

}   // namespace ec2
}   // namespace cdb
}   // namespace nx
