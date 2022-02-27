// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <type_traits>

#include "service.h"

namespace nx::utils {

namespace detail {

NX_UTILS_API void installProcessHandlers();
NX_UTILS_API void uninstallProcessHandlers();
NX_UTILS_API void registerServiceInstance(Service* service);

} // namespace detail

//-------------------------------------------------------------------------------------------------

template<typename ServiceType>
// requires std::is_base_of_v<Service, ServiceType>
int runService(int argc, char* argv[])
{
    detail::installProcessHandlers();

    ServiceType service(argc, argv);
    detail::registerServiceInstance(&service);
    const int result = service.exec();
    detail::registerServiceInstance(nullptr);

    detail::uninstallProcessHandlers();
    return result;
}

} // namespace nx::utils
