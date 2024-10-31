// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api {

struct NX_VMS_API ResourceGroup
{
    nx::Uuid id;

    /**%apidoc
     * %example All Devices
     */
    QString name;

    /**%apidoc
     * %example All Devices in this VMS Site.
     */
    QString description;

    /**%apidoc Whether this group comes with the Site and can not be removed. */
    bool isPredefined = false;
};
#define ResourceGroup_Fields \
    (id) \
    (name) \
    (description) \
    (isPredefined)
QN_FUSION_DECLARE_FUNCTIONS(ResourceGroup, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(ResourceGroup, ResourceGroup_Fields)

NX_REFLECTION_ENUM_CLASS(ResourceApiGroup,
    analyticsEngines,
    analyticsIntegrations,
    devices,
    layouts,
    servers,
    storages,
    users,
    videoWalls,
    virtualDevices,
    webPages
);

struct NX_VMS_API ResourceApiInfo
{
    nx::Uuid id;
    nx::Uuid typeId;
    ResourceApiGroup group{ResourceApiGroup::devices};
    std::string api;

    bool operator==(const ResourceApiInfo& other) const = default;

    nx::Uuid getId() const { return id; }
};
#define ResourceApiInfo_Fields (id)(typeId)(group)(api)
QN_FUSION_DECLARE_FUNCTIONS(ResourceApiInfo, (json), NX_VMS_API)
NX_REFLECTION_INSTRUMENT(ResourceApiInfo, ResourceApiInfo_Fields)

} // namespace nx::vms::api
