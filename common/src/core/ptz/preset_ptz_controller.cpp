#include "preset_ptz_controller.h"

#include <cassert>

#include <boost/range/algorithm/find_if.hpp>

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
    // TODO: don't use usage helper, use sync api
    // TODO: mutex

    d->helper = new QnStringKvPairUsageHelper(baseController->resource(), lit(""), QString(), this);
}

QnPresetPtzController::~QnPresetPtzController() {
    return;
}

bool QnPresetPtzController::extends(const QnPtzControllerPtr &baseController) {
    return 
        baseController->hasCapabilities(Qn::AbsolutePtzCapabilities) &&
        !baseController->hasCapabilities(Qn::PtzPresetCapability);
}

Qn::PtzCapabilities QnPresetPtzController::getCapabilities() {
    return base_type::getCapabilities() | Qn::PtzPresetCapability;
}

bool QnPresetPtzController::createPreset(const QnPtzPreset &preset, QString *presetId) {
    d->loadRecords();

    QString id = preset.id;
    if(id.isEmpty())
        id = QUuid::createUuid().toString();

    int index = d->records.indexOf(id);
    if(index == -1) {
        d->records.push_back(QnPtzPresetRecord());
        index = d->records.size() - 1;
    }
    QnPtzPresetRecord &record = d->records[index];
        
    record.preset.id = id;
    record.preset.name = preset.name;
    record.data.space = hasCapabilities(Qn::LogicalCoordinateSpaceCapability) ? Qn::LogicalCoordinateSpace : Qn::DeviceCoordinateSpace;
    if(!getPosition(record.data.space, &record.data.position))
        return false;

    d->saveRecords();
    *presetId = id;
    return true;
}

bool QnPresetPtzController::updatePreset(const QnPtzPreset &preset) {
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
    d->loadRecords();

    int index = d->records.indexOf(presetId);
    if(index == -1)
        return false;
    d->records.removeAt(index);

    d->saveRecords();
    return true;
}

bool QnPresetPtzController::activatePreset(const QString &presetId) {
    d->loadRecords();

    int index = d->records.indexOf(presetId);
    if(index == -1)
        return false;
    const QnPtzPresetRecord &record = d->records[index];

    if(!absoluteMove(record.data.space, record.data.position))    
        return false;

    return true;
}

bool QnPresetPtzController::getPresets(QnPtzPresetList *presets) {
    d->loadRecords();

    presets->clear();
    foreach(const QnPtzPresetRecord &record, d->records)
        presets->push_back(record.preset);

    return true;
}

