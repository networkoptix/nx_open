#pragma once

#include <QtGui/QVector3D>

#include <core/ptz/ptz_preset.h>
#include <nx/fusion/model_functions_fwd.h>

namespace ec2 {
namespace migration {
namespace ptz {

struct OldPtzPresetData
{
    QVector3D position;
    Qn::PtzCoordinateSpace space = Qn::DevicePtzCoordinateSpace;
    bool isValid = true;
};

#define OldPtzPresetData_Fields (position)(space)
QN_FUSION_DECLARE_FUNCTIONS(OldPtzPresetData, (json)(eq));

struct OldPtzPresetRecord
{
    OldPtzPresetRecord() = default;
    OldPtzPresetRecord(const QnPtzPreset &preset, const OldPtzPresetData &data):
        preset(preset),
        data(data)
    {
    }

    QnPtzPreset preset;
    OldPtzPresetData data;
};

#define OldPtzPresetRecord_Fields (preset)(data)
QN_FUSION_DECLARE_FUNCTIONS(OldPtzPresetRecord, (json)(eq));

using OldPtzPresetRecordHash = QHash<QString, OldPtzPresetRecord>;

} // namespace ptz
} // namespace migration
} // namespace ec2

Q_DECLARE_METATYPE(ec2::migration::ptz::OldPtzPresetRecordHash);
