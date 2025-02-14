// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>

#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/fusion/serialization/json_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api::rules {

/** Common type for UUID selection field. Stores the result of camera/user selection dialog.*/
struct UuidSelection
{
    /**%apidoc[opt]:uuidArray Manually selected ids. */
    QSet<nx::Uuid> ids;

    /**%apidoc[opt] Accept/target all flag. */
    bool all = false;

    bool operator==(const UuidSelection&) const = default;
};

#define UuidSelection_Fields (ids)(all)
QN_FUSION_DECLARE_FUNCTIONS(UuidSelection, (json), NX_VMS_API);
NX_REFLECTION_INSTRUMENT(UuidSelection, UuidSelection_Fields)

} // namespace nx::vms::api::rules
