#pragma once

#include <nx/utils/move_only_func.h>
#include <nx/update/info/update_request_data.h>

namespace nx {
namespace update {
namespace manager {
namespace detail {

class NX_UPDATE_API UpdateFileRequestDataFactory
{
public:
    using FactoryFunc = utils::MoveOnlyFunc<update::info::UpdateFileRequestData()>;

    static update::info::UpdateFileRequestData create(bool isClient);
    static void setFactoryFunc(FactoryFunc factoryFunc);
private:
    static FactoryFunc s_factoryFunc;
};

} // namespace detail
} // namespace manager
} // namespace update
} // namespace nx
