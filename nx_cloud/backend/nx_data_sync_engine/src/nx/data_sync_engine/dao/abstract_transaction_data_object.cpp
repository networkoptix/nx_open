#include "abstract_transaction_data_object.h"

#include <nx/utils/std/cpp14.h>

#include "rdb/transaction_data_object.h"
#include "memory/transaction_data_object_in_memory.h"

namespace nx {
namespace data_sync_engine {
namespace dao {

TransactionDataObjectFactory::TransactionDataObjectFactory():
    base_type(std::bind(&TransactionDataObjectFactory::defaultFactoryFunction, this,
        std::placeholders::_1))
{
}

TransactionDataObjectFactory& TransactionDataObjectFactory::instance()
{
    static TransactionDataObjectFactory factory;
    return factory;
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

std::unique_ptr<AbstractTransactionDataObject>
    TransactionDataObjectFactory::defaultFactoryFunction(int commandFormatVersion)
{
    return std::make_unique<rdb::TransactionDataObject>(commandFormatVersion);
}

} // namespace dao
} // namespace data_sync_engine
} // namespace nx
