#include "abstract_transaction_data_object.h"

#include <nx/utils/std/cpp14.h>

#include "rdb/transaction_data_object.h"

namespace nx {
namespace cdb {
namespace ec2 {
namespace dao {

static TransactionDataObjectFactory::FactoryFunc factoryFunc;

std::unique_ptr<AbstractTransactionDataObject> TransactionDataObjectFactory::create()
{
    if (factoryFunc)
        return factoryFunc();
    return std::make_unique<rdb::TransactionDataObject>();
}

void TransactionDataObjectFactory::setFactoryFunc(
    TransactionDataObjectFactory::FactoryFunc func)
{
    factoryFunc = std::move(func);
}

} // namespace dao
} // namespace ec2
} // namespace cdb
} // namespace nx
