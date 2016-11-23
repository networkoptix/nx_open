#include "abstract_transaction_data_object.h"

#include <nx/utils/std/cpp14.h>

#include "rdb/transaction_data_object.h"

namespace nx {
namespace cdb {
namespace ec2 {
namespace dao {

static AbstractTransactionDataObject::FactoryFunc factoryFunc;

std::unique_ptr<AbstractTransactionDataObject> AbstractTransactionDataObject::create()
{
    if (factoryFunc)
        return factoryFunc();
    return std::make_unique<rdb::TransactionDataObject>();
}

void AbstractTransactionDataObject::setFactoryFunc(
    AbstractTransactionDataObject::FactoryFunc func)
{
    factoryFunc = std::move(func);
}

} // namespace dao
} // namespace ec2
} // namespace cdb
} // namespace nx
