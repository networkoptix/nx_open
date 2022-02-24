// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/uuid.h>

namespace nx::vms::rules {

// TODO: #amalov Select a better place for field types. Consider API library.

/** Common type for UUID selection field. Stores the result of camera/user selection dialog.*/
struct UuidSelection
{
    /** Manually selected ids.*/
    QnUuidList ids;

    /** Accept/target all flag. */
    bool all = false;

    bool operator==(const UuidSelection&) const = default;
};

} // namespace nx::vms::rules

Q_DECLARE_METATYPE(nx::vms::rules::UuidSelection)
