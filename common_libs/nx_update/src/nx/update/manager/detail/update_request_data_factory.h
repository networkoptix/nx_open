#pragma once

#include <nx/utils/move_only_func.h>
#include <nx/utils/software_version.h>
#include <nx/update/info/update_request_data.h>

namespace nx {
namespace update {
namespace manager {
namespace detail {

class NX_UPDATE_API UpdateRequestDataFactory
{
public:
    using FactoryFunc = utils::MoveOnlyFunc<update::info::UpdateRequestData()>;

    static update::info::UpdateRequestData create(bool isClient,
        const nx::utils::SoftwareVersion *targetVersion);

    static void setFactoryFunc(FactoryFunc factoryFunc);

private:
    static FactoryFunc s_factoryFunc;
};

} // namespace detail
} // namespace manager
} // namespace update
} // namespace nx
