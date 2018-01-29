#pragma once

#include <nx/utils/move_only_func.h>
#include <nx/update/info/update_request_data.h>

namespace nx {
namespace mediaserver {
namespace updates2 {
namespace detail {

class UpdateFileRequestDataFactory
{
public:
    using FactoryFunc = utils::MoveOnlyFunc<update::info::UpdateFileRequestData()>;

    static update::info::UpdateFileRequestData create();
    static void setFactoryFunc(FactoryFunc factoryFunc);
private:
    static FactoryFunc s_factoryFunc;
};

} // namespace detail
} // namespace updates2
} // namespace mediaserver
} // namespace nx
