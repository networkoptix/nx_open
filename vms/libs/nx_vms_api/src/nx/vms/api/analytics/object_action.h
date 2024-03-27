// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/serialization/qjson.h>
#include <nx/vms/api/analytics/pixel_format.h>

namespace nx::vms::api::analytics {

NX_REFLECTION_ENUM_CLASS(ObjectActionCapability,
    noCapabilities = 0,
    needBestShotVideoFrame = 1 << 0,
    needBestShotObjectMetadata = 1 << 1,
    needFullTrack = 1 << 2,
    needBestShotImage = 1 << 3
)
Q_DECLARE_FLAGS(ObjectActionCapabilities, ObjectActionCapability)

struct ObjectActionRequirements
{
    /**%apidoc[opt] */
    ObjectActionCapabilities capabilities;

    // TODO: Investigate what happens if the Manifest is missing the value for this field -
    // it seems it will remain PixelFormat::undefined and an assertion will fail in
    // apiToAvPixelFormat().
    PixelFormat bestShotVideoFramePixelFormat;

    bool operator==(const ObjectActionRequirements& other) const = default;
};
#define nx_vms_api_analytics_Engine_ObjectAction_Requirements_Fields \
    (capabilities)(bestShotVideoFramePixelFormat)

struct ObjectAction
{
    QString id;
    QString name;

    QList<QString> supportedObjectTypeIds;
    QJsonObject parametersModel;

    ObjectActionRequirements requirements;

    bool operator==(const ObjectAction& other) const = default;
};
#define ObjectAction_Fields (id)(name)(supportedObjectTypeIds)(parametersModel) \
    (requirements)

QN_FUSION_DECLARE_FUNCTIONS(ObjectAction, (json), NX_VMS_API)
Q_DECLARE_OPERATORS_FOR_FLAGS(ObjectActionCapabilities)
QN_FUSION_DECLARE_FUNCTIONS(ObjectActionRequirements, (json), NX_VMS_API)

NX_REFLECTION_INSTRUMENT(ObjectActionRequirements, nx_vms_api_analytics_Engine_ObjectAction_Requirements_Fields);
NX_REFLECTION_INSTRUMENT(ObjectAction, ObjectAction_Fields);

} // namespace nx::vms::api::analytics
