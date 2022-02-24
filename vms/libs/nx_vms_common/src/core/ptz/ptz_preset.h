// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtGui/QVector3D>

#include "ptz_fwd.h"

#include <core/ptz/ptz_preset.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>

#include <nx/vms/common/ptz/coordinate_space.h>
#include <nx/vms/common/ptz/vector.h>

static const QString kPresetsPropertyKey = "ptzPresets";
static const QString kPtzPresetMappingPropertyName = "presetMapping";

namespace nx::core::ptz {

NX_REFLECTION_ENUM_CLASS(PresetType,
    undefined,
    system,
    native
)

} // namespace nx::core::ptz

Q_DECLARE_METATYPE(nx::core::ptz::PresetType)

struct NX_VMS_COMMON_API QnPtzPreset
{
    Q_GADGET
public:
    QnPtzPreset() {}
    QnPtzPreset(const QString& id, const QString& name):
        id(id),
        name(name)
    {
    }

    QString id;
    QString name;

    bool operator==(const QnPtzPreset& other) const = default;
};

#define QnPtzPreset_Fields (id)(name)
QN_FUSION_DECLARE_FUNCTIONS(QnPtzPreset, (json), NX_VMS_COMMON_API);

Q_DECLARE_METATYPE(QnPtzPreset)
Q_DECLARE_METATYPE(QnPtzPresetList)

struct QnPtzPresetData
{
    QnPtzPresetData(): space(nx::vms::common::ptz::CoordinateSpace::device) {}

    bool operator==(const QnPtzPresetData& other) const = default;

    nx::vms::common::ptz::Vector position;
    nx::vms::common::ptz::CoordinateSpace space;
};

#define QnPtzPresetData_Fields (position)(space)
QN_FUSION_DECLARE_FUNCTIONS(QnPtzPresetData, (json));

struct QnPtzPresetRecord
{
    QnPtzPresetRecord() {}
    QnPtzPresetRecord(
        const QnPtzPreset &preset,
        const QnPtzPresetData &data)
        :
        preset(preset),
        data(data)
    {
    }

    bool operator==(const QnPtzPresetRecord& other) const = default;

    QnPtzPreset preset;
    QnPtzPresetData data;
};

#define QnPtzPresetRecord_Fields (preset)(data)
QN_FUSION_DECLARE_FUNCTIONS(QnPtzPresetRecord, (json), NX_VMS_COMMON_API);

using QnPtzPresetRecordHash = QHash<QString, QnPtzPresetRecord>;

using QnPtzPresetMapping = QMap<QString, QnPtzPreset>;

Q_DECLARE_METATYPE(QnPtzPresetRecordHash);
