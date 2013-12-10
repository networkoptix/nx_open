#include "preset_ptz_controller.h"

#include <cassert>

#include <QtCore/QMutex>

#include <api/kvpair_usage_helper.h>
#include <utils/common/json.h>

#include "ptz_preset.h"


// -------------------------------------------------------------------------- //
// Model Data
// -------------------------------------------------------------------------- //
struct QnPtzPresetData {
    QnPtzPresetData(): space(Qn::DeviceCoordinateSpace) {}

    QVector3D position;
    Qn::PtzCoordinateSpace space;
};
QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnPtzPresetData, (position)(space))

struct QnPtzPresetRecord {
    QnPtzPresetRecord() {}
    QnPtzPresetRecord(const QnPtzPreset &preset, const QnPtzPresetData &data): preset(preset), data(data) {}

    QnPtzPreset preset;
    QnPtzPresetData data;
};
QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnPtzPresetRecord, (preset)(data))


// -------------------------------------------------------------------------- //
// QnPresetPtzControllerPrivate
// -------------------------------------------------------------------------- //
class QnPresetPtzControllerPrivate {
public:
    QnPresetPtzControllerPrivate(): helper(NULL) {}
    
    void loadRecords() {
        if(!records.isEmpty())
            return;

        if(helper->value().isEmpty()) {
            records.clear();
        } else {
            QJson::deserialize(helper->value().toUtf8(), &records);
        }
    }

    void saveRecords() {
        helper->setValue(QString::fromUtf8(QJson::serialized(records)));
    }

    QMutex mutex;
    QnStringKvPairUsageHelper *helper;
    QHash<QString, QnPtzPresetRecord> records;
};


// -------------------------------------------------------------------------- //
// QnPresetPtzController
// -------------------------------------------------------------------------- //
QnPresetPtzController::QnPresetPtzController(const QnPtzControllerPtr &baseController): 
    base_type(baseController),
    d(new QnPresetPtzControllerPrivate())
{
    // TODO: don't use usage helper, use sync api?

    d->helper = new QnStringKvPairUsageHelper(resource(), lit("ptzPresets"), QString(), this);
}

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
    data.space = hasCapabilities(Qn::LogicalPositioningPtzCapability) ? Qn::LogicalCoordinateSpace : Qn::DeviceCoordinateSpace;
    if(!getPosition(data.space, &data.position))
        return false;

    QMutexLocker locker(&d->mutex);
    d->loadRecords();
    d->records.insert(preset.id, QnPtzPresetRecord(preset, data));
    d->saveRecords();
    return true;
}

bool QnPresetPtzController::updatePreset(const QnPtzPreset &preset) {
    QMutexLocker locker(&d->mutex);

    d->loadRecords();

    if(!d->records.contains(preset.id))
        return false;

    QnPtzPresetRecord &record = d->records[preset.id];
    if(record.preset == preset)
        return true; /* No need to save it. */
    record.preset = preset;

    d->saveRecords();
    return true;
}

bool QnPresetPtzController::removePreset(const QString &presetId) {
    QMutexLocker locker(&d->mutex);

    d->loadRecords();
    if(d->records.remove(presetId) == 0)
        return false;
    d->saveRecords();
    return true;
}

bool QnPresetPtzController::activatePreset(const QString &presetId) {
    QnPtzPresetData data;
    {
        QMutexLocker locker(&d->mutex);
        d->loadRecords();

        if(!d->records.contains(presetId))
            return false;
        data = d->records[presetId].data;

        qDebug() << "PRESET" << d->records[presetId].preset.name << d->records[presetId].data.position;
    }


    if(!absoluteMove(data.space, data.position))    
        return false;

    return true;
}

bool QnPresetPtzController::getPresets(QnPtzPresetList *presets) {
    QMutexLocker locker(&d->mutex);

    d->loadRecords();
    presets->clear();
    foreach(const QnPtzPresetRecord &record, d->records)
        presets->push_back(record.preset);

    return true;
}

