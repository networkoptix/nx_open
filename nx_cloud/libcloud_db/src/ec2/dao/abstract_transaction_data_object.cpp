#include "abstract_transaction_data_object.h"

#include <nx/utils/std/cpp14.h>

#include "rdb/transaction_data_object.h"
#include "memory/transaction_data_object_in_memory.h"

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

void TransactionDataObjectFactory::setDataObjectType(DataObjectType dataObjectType)
{
    switch (dataObjectType)
    {
        case DataObjectType::rdbms:
            setDataObjectType<rdb::TransactionDataObject>();
            break;

        case DataObjectType::ram:
            setDataObjectType<memory::TransactionDataObject>();
            break;

        default:
            NX_ASSERT(false);
            break;
    }
}

void TransactionDataObjectFactory::setFactoryFunc(
    TransactionDataObjectFactory::FactoryFunc func)
{
    factoryFunc = std::move(func);
}

void TransactionDataObjectFactory::resetToDefaultFactory()
{
    factoryFunc = nullptr;
}

} // namespace dao
} // namespace ec2
} // namespace cdb
} // namespace nx
