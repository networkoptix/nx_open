// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/vms/api/analytics/manifest_items.h>

namespace nx::vms::api::analytics {

struct NX_VMS_API TypeLibrary
{
    /**%apidoc[opt] */
    QList<EventType> eventTypes;

    /**%apidoc[opt] */
    QList<ObjectType> objectTypes;

    /**%apidoc[opt] */
    QList<Group> groups;

    /**%apidoc[opt] */
    QList<EnumType> enumTypes;

    /**%apidoc[opt] */
    QList<ColorType> colorTypes;

    /**%apidoc[opt] */
    QList<HiddenExtendedType> extendedObjectTypes;

    /**%apidoc[opt] */
    QList<HiddenExtendedType> extendedEventTypes;

    /**%apidoc[opt] */
    QList<AttributesWithId> attributeLists;

    bool operator==(const TypeLibrary& other) const = default;
};

#define TypeLibrary_Fields \
    (eventTypes) \
    (objectTypes) \
    (enumTypes) \
    (colorTypes) \
    (groups) \
    (extendedObjectTypes) \
    (extendedEventTypes) \
    (attributeLists)

NX_REFLECTION_INSTRUMENT(TypeLibrary, TypeLibrary_Fields);

QN_FUSION_DECLARE_FUNCTIONS(TypeLibrary, (json), NX_VMS_API)

} // namespace nx::vms::api::analytics
