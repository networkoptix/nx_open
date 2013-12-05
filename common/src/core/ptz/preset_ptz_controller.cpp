#include "preset_ptz_controller.h"

#include <cassert>

#include <common/common_serialization.h>

#include <utils/common/json.h>

#include <api/kvpair_usage_helper.h>

// -------------------------------------------------------------------------- //
// Model Data
// -------------------------------------------------------------------------- //
struct QnPtzPresetData {
    QnPtzPresetData(): space(Qn::DeviceCoordinateSpace) {}

    QVector3D position;
    Qn::PtzCoordinateSpace space;
};
QN_DEFINE_STRUCT_JSON_SERIALIZATION_FUNCTIONS(QnPtzPresetData, (position)(space))


// -------------------------------------------------------------------------- //
// QnPresetPtzController
// -------------------------------------------------------------------------- //
QnPresetPtzController::QnPresetPtzController(const QnPtzControllerPtr &baseController): 
    base_type(baseController),
    m_helper(new QnStringKvPairUsageHelper(baseController->resource(), lit(""), QString(), this))
{
    assert(baseController->hasCapabilities(Qn::AbsolutePtzCapabilities));
}

Qn::PtzCapabilities QnPresetPtzController::getCapabilities() {
    return base_type::getCapabilities() | Qn::PtzPresetCapability;
}

bool QnPresetPtzController::createPreset(QnPtzPreset *preset) {
    return false;
}

bool QnPresetPtzController::removePreset(const QnPtzPreset &preset) {
    return false;
}

bool QnPresetPtzController::activatePreset(const QnPtzPreset &preset) {
    return false;
}

bool QnPresetPtzController::getPresets(QnPtzPresetList *presets) {
    return false;
}
