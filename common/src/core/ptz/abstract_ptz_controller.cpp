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
    
    if((fields & Qn::DevicePositionPtzField)    && getPosition(Qn::DevicePtzCoordinateSpace, &data->devicePosition))    data->fields |= Qn::DevicePositionPtzField;
    if((fields & Qn::LogicalPositionPtzField)   && getPosition(Qn::LogicalPtzCoordinateSpace, &data->logicalPosition))  data->fields |= Qn::LogicalPositionPtzField;
    if((fields & Qn::DeviceLimitsPtzField)      && getLimits(Qn::DevicePtzCoordinateSpace, &data->deviceLimits))        data->fields |= Qn::DeviceLimitsPtzField;
    if((fields & Qn::LogicalLimitsPtzField)     && getLimits(Qn::LogicalPtzCoordinateSpace, &data->logicalLimits))      data->fields |= Qn::LogicalLimitsPtzField;
    if((fields & Qn::FlipPtzField)              && getFlip(&data->flip))                                             data->fields |= Qn::FlipPtzField;
    if((fields & Qn::PresetsPtzField)           && getPresets(&data->presets))                                       data->fields |= Qn::PresetsPtzField;
    if((fields & Qn::ToursPtzField)             && getTours(&data->tours))                                           data->fields |= Qn::ToursPtzField;
}

