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
    QnPtzPreset preset;
    QnPtzPresetData data;
};
QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnPtzPresetRecord, (preset)(data))

class QnPtzPresetRecordList: public QList<QnPtzPresetRecord> {
    typedef QList<QnPtzPresetRecord> base_type;
public:

    using base_type::indexOf;
    int indexOf(const QString &id) {
        for(int i = 0; i < size(); i++)
            if(this->operator[](i).preset.id == id)
                return i;
        return -1;
    }
};


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
            QJson::deserialize<QnPtzPresetRecordList>(helper->value().toUtf8(), &records);
        }

    }

    void saveRecords() {
        helper->setValue(QString::fromUtf8(QJson::serialized(records)));
    }

    QMutex mutex;
    QnPtzPresetRecordList records;
    QnStringKvPairUsageHelper *helper;
};


// -------------------------------------------------------------------------- //
// QnPresetPtzController
// -------------------------------------------------------------------------- //
QnPresetPtzController::QnPresetPtzController(const QnPtzControllerPtr &baseController): 
    base_type(baseController),
    d(new QnPresetPtzControllerPrivate())
{
    // TODO: don't use usage helper, use sync api?

    d->helper = new QnStringKvPairUsageHelper(baseController->resource(), lit(""), QString(), this);
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

    int index = d->records.indexOf(preset.id);
    if(index == -1) {
        d->records.push_back(QnPtzPresetRecord());
        index = d->records.size() - 1;
    }
    QnPtzPresetRecord &record = d->records[index];

    record.preset = preset;
    record.data = data;

    d->saveRecords();
    return true;
}

bool QnPresetPtzController::updatePreset(const QnPtzPreset &preset) {
    QMutexLocker locker(&d->mutex);

    d->loadRecords();

    int index = d->records.indexOf(preset.id);
    if(index == -1)
        return false;

    QnPtzPresetRecord &record = d->records[index];
    if(record.preset == preset)
        return true; /* No need to save it. */

    record.preset = preset;

    d->saveRecords();
    return true;
}

bool QnPresetPtzController::removePreset(const QString &presetId) {
    QMutexLocker locker(&d->mutex);

    d->loadRecords();

    int index = d->records.indexOf(presetId);
    if(index == -1)
        return false;
    d->records.removeAt(index);

    d->saveRecords();
    return true;
}

bool QnPresetPtzController::activatePreset(const QString &presetId) {
    QnPtzPresetData data;
    {
        QMutexLocker locker(&d->mutex);
        d->loadRecords();

        int index = d->records.indexOf(presetId);
        if(index == -1)
            return false;
        data = d->records[index].data;
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

