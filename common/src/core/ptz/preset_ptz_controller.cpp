#include "preset_ptz_controller.h"

#include <utils/common/model_functions.h>
#include <api/app_server_connection.h>
#include <api/resource_property_adaptor.h>
#include <core/resource/resource.h>

#include "ptz_preset.h"

namespace {
    const QString propertyKey = lit("ptzPresets");
}

// -------------------------------------------------------------------------- //
// Model Data
// -------------------------------------------------------------------------- //
struct QnPtzPresetData {
    QnPtzPresetData(): space(Qn::DevicePtzCoordinateSpace) {}

    QVector3D position;
    Qn::PtzCoordinateSpace space;
};
QN_DEFINE_STRUCT_FUNCTIONS(QnPtzPresetData, (json)(eq), (position)(space))

struct QnPtzPresetRecord {
    QnPtzPresetRecord() {}
    QnPtzPresetRecord(const QnPtzPreset &preset, const QnPtzPresetData &data): preset(preset), data(data) {}

    QnPtzPreset preset;
    QnPtzPresetData data;
};
QN_DEFINE_STRUCT_FUNCTIONS(QnPtzPresetRecord, (json)(eq), (preset)(data))

Q_DECLARE_METATYPE(QnPtzPresetRecordHash)


// -------------------------------------------------------------------------- //
// QnPresetPtzController
// -------------------------------------------------------------------------- //
QnPresetPtzController::QnPresetPtzController(const QnPtzControllerPtr &baseController): 
    base_type(baseController),
    m_adaptor(new QnJsonResourcePropertyAdaptor<QnPtzPresetRecordHash>(baseController->resource(), lit("ptzPresets"), this))
{}

QnPresetPtzController::~QnPresetPtzController() {
    return;
}

bool QnPresetPtzController::extends(const QnPtzControllerPtr &baseController) {
    return 
        baseController->hasCapabilities(Qn::AbsolutePtzCapabilities) &&
        (baseController->hasCapabilities(Qn::DevicePositioningPtzCapability) || baseController->hasCapabilities(Qn::LogicalPositioningPtzCapability)) &&
        !baseController->hasCapabilities(Qn::PresetsPtzCapability);
}

Qn::PtzCapabilities QnPresetPtzController::getCapabilities() {
    /* Note that this controller preserves the Qn::NonBlockingPtzCapability. */
    return base_type::getCapabilities() | Qn::PresetsPtzCapability;
}

bool QnPresetPtzController::createPreset(const QnPtzPreset &preset) {
    if(preset.id.isEmpty())
        return false;

    QnPtzPresetData data;
    data.space = hasCapabilities(Qn::LogicalPositioningPtzCapability) ? Qn::LogicalPtzCoordinateSpace : Qn::DevicePtzCoordinateSpace;
    if(!getPosition(data.space, &data.position))
        return false;

    QMutexLocker locker(&m_mutex);
    
    QnPtzPresetRecordHash records = m_adaptor->value();
    records.insert(preset.id, QnPtzPresetRecord(preset, data));
    m_adaptor->setValue(records);
    
    return true;
}

bool QnPresetPtzController::updatePreset(const QnPtzPreset &preset) {
    QMutexLocker locker(&m_mutex);

    QnPtzPresetRecordHash records = m_adaptor->value();
    if(!records.contains(preset.id))
        return false;

    QnPtzPresetRecord &record = records[preset.id];
    if(record.preset == preset)
        return true; /* No need to save it. */
    record.preset = preset;
    
    m_adaptor->setValue(records);
    return true;
}

bool QnPresetPtzController::removePreset(const QString &presetId) {
    QMutexLocker locker(&m_mutex);

    QnPtzPresetRecordHash records = m_adaptor->value();
    if(records.remove(presetId) == 0)
        return false;

    m_adaptor->setValue(records);
    return true;
}

bool QnPresetPtzController::activatePreset(const QString &presetId, qreal speed) {
    QnPtzPresetData data;
    {
        QMutexLocker locker(&m_mutex);

        const QnPtzPresetRecordHash &records = m_adaptor->value();
        if(!records.contains(presetId))
            return false;
        data = records.value(presetId).data;
    }

    if(!absoluteMove(data.space, data.position, speed))
        return false;

    return true;
}

bool QnPresetPtzController::getPresets(QnPtzPresetList *presets) {
    QMutexLocker locker(&m_mutex);

    presets->clear();
    foreach(const QnPtzPresetRecord &record, m_adaptor->value())
        presets->push_back(record.preset);

    return true;
}

