// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QStringList>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/data_macros.h>

namespace nx::vms::api {

struct NX_VMS_API ObjectTypeSettings
{
    bool isAllObjectTypes = false;
    QStringList objectTypeIds;

    bool operator==(const ObjectTypeSettings& other) const;
    QByteArray toString() const;

    bool empty() const;
    bool contains(const QString& typeId) const;
};

#define ObjectTypeSettings_Fields (isAllObjectTypes)(objectTypeIds)
NX_REFLECTION_INSTRUMENT(ObjectTypeSettings, ObjectTypeSettings_Fields)

QN_FUSION_DECLARE_FUNCTIONS(ObjectTypeSettings, (json), NX_VMS_API)

struct NX_VMS_API PixelationSettings: public ObjectTypeSettings
{
    double intensity = 1.0; /**<%apidoc[opt]:float */
    QSet<nx::Uuid> excludeCameraIds; /**<%apidoc:uuidArray */

    bool operator==(const PixelationSettings& other) const;
    QByteArray toString() const;

    bool isPixelationRequiredForCamera(nx::Uuid cameraId) const;
};

#define PixelationSettings_Fields ObjectTypeSettings_Fields(intensity)(excludeCameraIds)
NX_REFLECTION_INSTRUMENT(PixelationSettings, PixelationSettings_Fields)

QN_FUSION_DECLARE_FUNCTIONS(PixelationSettings, (json), NX_VMS_API)

} // namespace nx::vms::api
