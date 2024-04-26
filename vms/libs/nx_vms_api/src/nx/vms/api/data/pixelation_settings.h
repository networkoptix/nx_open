// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QStringList>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/data_macros.h>

namespace nx::vms::api {

struct NX_VMS_API PixelationSettings
{
    bool isAllObjectTypes = false;
    QStringList objectTypeIds;
    double intensity = 1.0; /**<%apidoc[opt]:float */
    QSet<nx::Uuid> excludeCameraIds; /**<%apidoc:uuidArray */

    bool operator==(const PixelationSettings& other) const;
};

#define PixelationSettings_Fields (isAllObjectTypes)(objectTypeIds)(intensity)(excludeCameraIds)
NX_REFLECTION_INSTRUMENT(PixelationSettings, PixelationSettings_Fields)

QN_FUSION_DECLARE_FUNCTIONS(PixelationSettings, (json), NX_VMS_API)

} // namespace nx::vms::api
