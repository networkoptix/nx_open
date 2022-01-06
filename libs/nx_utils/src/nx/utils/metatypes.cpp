// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "metatypes.h"

#include <atomic>

#include <QtCore/QDataStream>

#include "buffer.h"
#include "mac_address.h"
#include "url.h"
#include "uuid.h"
#include "scope_guard.h"
#include "async_handler_executor_detail.h"

template<class Rep, class Period>
QDataStream& operator<<(QDataStream& out, const std::chrono::duration<Rep, Period>& duration)
{
    // GCC and Clang compilers uses long int as duration representation, as QDataStream does not
    // have appropriate operator<< it is required to cast it manually.
    out << static_cast<long long>(duration.count());
    return out;
}

template<class Rep, class Period>
QDataStream& operator>>(QDataStream& in, std::chrono::duration<Rep, Period>& duration)
{
    long long count;
    in >> count;
    duration = std::chrono::duration<Rep, Period>(count);
    return in;
}

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

    qRegisterMetaType<QnUuid>();
    qRegisterMetaType<QSet<QnUuid>>("QSet<QnUuid>");

    qRegisterMetaType<SharedGuardPtr>();

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
