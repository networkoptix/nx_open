// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "metatypes.h"

#include <nx/fusion/serialization/json_functions.h>

#include "field_types.h"

namespace nx::vms::rules {

// TODO: Consider moving this registration to the API library.
void Metatypes::initialize()
{
    static std::atomic_bool initialized = false;

    if (initialized.exchange(true))
        return;

    qRegisterMetaType<QnUuidList>("QnUuidList");
    QnJsonSerializer::registerSerializer<QnUuidList>();

    QnJsonSerializer::registerSerializer<QSet<QnUuid>>();
};

} // namespace nx::vms::rules
