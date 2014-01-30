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
{
    m_asynchronous = baseController->hasCapabilities(Qn::AsynchronousPtzCapability);
    connect(this, &QnPresetPtzController::finishedLater, this, &QnAbstractPtzController::finished, Qt::QueuedConnection);
}

QnPresetPtzController::~QnPresetPtzController() {
    return;
}

bool QnPresetPtzController::extends(Qn::PtzCapabilities capabilities) {
    return 
        ((capabilities & Qn::AbsolutePtzCapabilities) == Qn::AbsolutePtzCapabilities) &&
        (capabilities & (Qn::DevicePositioningPtzCapability | Qn::LogicalPositioningPtzCapability)) &&
        !(capabilities & Qn::PresetsPtzCapability);
}

Qn::PtzCapabilities QnPresetPtzController::getCapabilities() {
    /* Note that this controller preserves both Qn::AsynchronousPtzCapability and Qn::SynchronizedPtzCapability. */
    Qn::PtzCapabilities capabilities = base_type::getCapabilities();
    return extends(capabilities) ? (capabilities | Qn::PresetsPtzCapability) : capabilities;
}

bool QnPresetPtzController::createPreset(const QnPtzPreset &preset) {
    if(preset.id.isEmpty())
        return false;

    QnPtzPresetData data;
    data.space = hasCapabilities(Qn::LogicalPositioningPtzCapability) ? Qn::LogicalPtzCoordinateSpace : Qn::DevicePtzCoordinateSpace;
    if(!getPosition(data.space, &data.position)) // TODO: #Elric this won't work for async base controller.
        return false;

    QMutexLocker locker(&m_mutex);
    
    QnPtzPresetRecordHash records = m_adaptor->value();
    records.insert(preset.id, QnPtzPresetRecord(preset, data));
    m_adaptor->setValue(records);
    
    if(m_asynchronous)
        emit finishedLater(Qn::CreatePresetPtzCommand, QVariant::fromValue(preset));
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

    if(m_asynchronous)
        emit finishedLater(Qn::UpdatePresetPtzCommand, QVariant::fromValue(preset));
    return true;
}

bool QnPresetPtzController::removePreset(const QString &presetId) {
    QMutexLocker locker(&m_mutex);

    QnPtzPresetRecordHash records = m_adaptor->value();
    if(records.remove(presetId) == 0)
        return false;

    m_adaptor->setValue(records);

    if(m_asynchronous)
        emit finishedLater(Qn::RemovePresetPtzCommand, QVariant::fromValue(presetId));
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

    // TODO: #Elric maybe emit finished when we get absoluteMove finished?

    if(m_asynchronous)
        emit finishedLater(Qn::ActivatePresetPtzCommand, QVariant::fromValue(presetId));
    return true;
}

bool QnPresetPtzController::getPresets(QnPtzPresetList *presets) {
    QMutexLocker locker(&m_mutex);

    presets->clear();
    foreach(const QnPtzPresetRecord &record, m_adaptor->value())
        presets->push_back(record.preset);

    if(m_asynchronous)
        emit finishedLater(Qn::GetPresetsPtzCommand, QVariant::fromValue(*presets));
    return true;
}

