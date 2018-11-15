#include "abstract_transaction_data_object.h"

#include <nx/utils/std/cpp14.h>

#include "rdb/transaction_data_object.h"
#include "memory/transaction_data_object_in_memory.h"

namespace nx::clusterdb::engine::dao {

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

TransactionDataObjectFactory::Function TransactionDataObjectFactory::setDataObjectType(DataObjectType dataObjectType)
{
    switch (dataObjectType)
    {
        case DataObjectType::rdbms:
            return setDataObjectType<rdb::TransactionDataObject>();

        case DataObjectType::ram:
            return setDataObjectType<memory::TransactionDataObject>();

        default:
            NX_ASSERT(false);
            return nullptr;
    }
}

std::unique_ptr<AbstractTransactionDataObject>
    TransactionDataObjectFactory::defaultFactoryFunction(int commandFormatVersion)
{
    return std::make_unique<rdb::TransactionDataObject>(commandFormatVersion);
}

} // namespace nx::clusterdb::engine::dao
