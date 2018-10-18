#pragma once

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtGui/QVector3D>

#include "ptz_fwd.h"

#include <common/common_globals.h>
#include <core/ptz/ptz_preset.h>
#include <nx/fusion/model_functions_fwd.h>

#include <nx/core/ptz/vector.h>

static const QString kPresetsPropertyKey = lit("ptzPresets");
static const QString kPtzPresetMappingPropertyName = lit("presetMapping");

namespace nx {
namespace core {
namespace ptz {

enum class PresetType
{
    undefined,
    system,
    native,
};

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PresetType);
} // namespace ptz
} // namespace core
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS(nx::core::ptz::PresetType, (metatype)(lexical));

struct QnPtzPreset
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
};

#define QnPtzPreset_Fields (id)(name)
QN_FUSION_DECLARE_FUNCTIONS(QnPtzPreset, (json)(eq));

Q_DECLARE_METATYPE(QnPtzPreset)
Q_DECLARE_METATYPE(QnPtzPresetList)

struct QnPtzPresetData
{
    QnPtzPresetData(): space(Qn::DevicePtzCoordinateSpace) {}

    nx::core::ptz::Vector position;
    Qn::PtzCoordinateSpace space;
};

#define QnPtzPresetData_Fields (position)(space)
QN_FUSION_DECLARE_FUNCTIONS(QnPtzPresetData, (json)(eq));

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

    QnPtzPreset preset;
    QnPtzPresetData data;
};

#define QnPtzPresetRecord_Fields (preset)(data)
QN_FUSION_DECLARE_FUNCTIONS(QnPtzPresetRecord, (json)(eq));

using QnPtzPresetRecordHash = QHash<QString, QnPtzPresetRecord>;

using QnPtzPresetMapping = QMap<QString, QnPtzPreset>;

Q_DECLARE_METATYPE(QnPtzPresetRecordHash);
