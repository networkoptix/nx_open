// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "metatypes.h"

#include <atomic>

#include "async_handler_executor_detail.h"
#include "buffer.h"
#include "mac_address.h"
#include "scope_guard.h"
#include "software_version.h"
#include "url.h"
#include "uuid.h"

namespace nx::utils {

void Metatypes::initialize()
{
    static std::atomic_bool initialized = false;

    if (initialized.exchange(true))
        return;

    // Fully qualified namespaces are not needed here but are mandatory in all signal declarations.
    qRegisterMetaType<Buffer>();

    qRegisterMetaType<MacAddress>();

    qRegisterMetaType<Url>();

    qRegisterMetaType<nx::Uuid>();
    qRegisterMetaType<QSet<nx::Uuid>>("QSet<nx::Uuid>");

    qRegisterMetaType<SharedGuardPtr>();
    qRegisterMetaType<SoftwareVersion>();

    qRegisterMetaType<async_handler_executor_detail::ExecuteArg>();

    qRegisterMetaType<std::chrono::hours>();
    qRegisterMetaType<std::chrono::minutes>();
    qRegisterMetaType<std::chrono::seconds>();
    qRegisterMetaType<std::chrono::milliseconds>();
    qRegisterMetaType<std::chrono::microseconds>();

    QMetaType::registerConverter<std::chrono::hours, std::chrono::minutes>();
    QMetaType::registerConverter<std::chrono::hours, std::chrono::seconds>();
    QMetaType::registerConverter<std::chrono::hours, std::chrono::milliseconds>();
    QMetaType::registerConverter<std::chrono::hours, std::chrono::microseconds>();
    QMetaType::registerConverter<std::chrono::minutes, std::chrono::seconds>();
    QMetaType::registerConverter<std::chrono::minutes, std::chrono::milliseconds>();
    QMetaType::registerConverter<std::chrono::minutes, std::chrono::microseconds>();
    QMetaType::registerConverter<std::chrono::seconds, std::chrono::milliseconds>();
    QMetaType::registerConverter<std::chrono::seconds, std::chrono::microseconds>();
    QMetaType::registerConverter<std::chrono::milliseconds, std::chrono::microseconds>();
};

} // namespace nx::utils
