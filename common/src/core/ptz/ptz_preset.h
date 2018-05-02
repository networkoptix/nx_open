#pragma once

#include <QtCore/QString>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtGui/QVector3D>

#include "ptz_fwd.h"

#include <common/common_globals.h>
#include <core/ptz/ptz_preset.h>
#include <nx/fusion/model_functions_fwd.h>

static const QString kPresetsPropertyKey = lit("ptzPresets");
static const QString kPtzPresetMappingPropertyName = lit("presetMapping");

struct QnPtzPreset
{
    Q_GADGET
public:
    QnPtzPreset() {}
    QnPtzPreset(
        const QString& id,
        const QString& name,
        const QString& idOnDevice = QString())
        :
        id(id),
        name(name),
        idOnDevice(idOnDevice)
    {
    }

    QString id;
    QString name;
    QString idOnDevice;
};
#define QnPtzPreset_Fields (id)(name)(idOnDevice)

Q_DECLARE_METATYPE(QnPtzPreset)
Q_DECLARE_METATYPE(QnPtzPresetList)

struct QnPtzPresetData
{
    QnPtzPresetData(): space(Qn::DevicePtzCoordinateSpace) {}

    QVector3D position;
    Qn::PtzCoordinateSpace space;
    bool isValid = true;
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
