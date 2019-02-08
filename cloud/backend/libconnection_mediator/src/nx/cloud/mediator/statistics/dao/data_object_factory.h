#pragma once

#include <memory>

#include <nx/utils/move_only_func.h>

#include "abstract_data_object.h"

namespace nx {
namespace hpm {
namespace stats {
namespace dao {

class DataObjectFactory
{
public:
    using FactoryFunc = nx::utils::MoveOnlyFunc<std::unique_ptr<AbstractDataObject>()>;

    static std::unique_ptr<AbstractDataObject> create();

    static void setFactoryFunc(FactoryFunc newFunc);
};

} // namespace dao
} // namespace stats
} // namespace hpm
} // namespace nx
