#include "abstract_ptz_controller.h"

#include <core/resource/resource.h>

QnAbstractPtzController::QnAbstractPtzController(const QnResourcePtr &resource): 
    m_resource(resource) 
{}

QnAbstractPtzController::~QnAbstractPtzController() {
    return;
}

void QnAbstractPtzController::getData(Qn::PtzDataFields fields, QnPtzData *data) {
    data->fields = Qn::NoPtzFields;
    data->capabilities = getCapabilities();
    
    if((fields & Qn::PtzDevicePositionField)    && getPosition(Qn::DeviceCoordinateSpace, &data->devicePosition))    data->fields |= Qn::PtzDevicePositionField;
    if((fields & Qn::PtzLogicalPositionField)   && getPosition(Qn::LogicalCoordinateSpace, &data->logicalPosition))  data->fields |= Qn::PtzLogicalPositionField;
    if((fields & Qn::PtzDeviceLimitsField)      && getLimits(Qn::DeviceCoordinateSpace, &data->deviceLimits))        data->fields |= Qn::PtzDeviceLimitsField;
    if((fields & Qn::PtzLogicalLimitsField)     && getLimits(Qn::LogicalCoordinateSpace, &data->logicalLimits))      data->fields |= Qn::PtzLogicalLimitsField;
    if((fields & Qn::PtzFlipField)              && getFlip(&data->flip))                                             data->fields |= Qn::PtzFlipField;
    if((fields & Qn::PtzPresetsField)           && getPresets(&data->presets))                                       data->fields |= Qn::PtzPresetsField;
    if((fields & Qn::PtzToursField)             && getTours(&data->tours))                                           data->fields |= Qn::PtzToursField;
}

