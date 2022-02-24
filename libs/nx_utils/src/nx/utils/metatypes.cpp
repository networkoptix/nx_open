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
    qRegisterMetaTypeStreamOperators<Url>();
    qRegisterMetaTypeStreamOperators<QList<Url>>();

    qRegisterMetaType<QnUuid>();
    qRegisterMetaType<QSet<QnUuid>>("QSet<QnUuid>");
    qRegisterMetaTypeStreamOperators<QnUuid>();

    qRegisterMetaType<SharedGuardPtr>();

    qRegisterMetaType<async_handler_executor_detail::ExecuteArg>();
};

} // namespace nx::utils
