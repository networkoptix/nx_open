#include "abstract_ptz_controller.h"

#include <core/resource/resource.h>

QnAbstractPtzController::QnAbstractPtzController(const QnResourcePtr &resource): 
    m_resource(resource) 
{}

QnAbstractPtzController::~QnAbstractPtzController() {
    return;
}

void QnAbstractPtzController::getData(Qn::PtzDataFields query, QnPtzData *data) {
    data->query = query;
    data->fields = Qn::NoPtzFields;
    data->capabilities = getCapabilities();
    
    if((query & Qn::DevicePositionPtzField)    && getPosition(Qn::DevicePtzCoordinateSpace, &data->devicePosition))     data->fields |= Qn::DevicePositionPtzField;
    if((query & Qn::LogicalPositionPtzField)   && getPosition(Qn::LogicalPtzCoordinateSpace, &data->logicalPosition))   data->fields |= Qn::LogicalPositionPtzField;
    if((query & Qn::DeviceLimitsPtzField)      && getLimits(Qn::DevicePtzCoordinateSpace, &data->deviceLimits))         data->fields |= Qn::DeviceLimitsPtzField;
    if((query & Qn::LogicalLimitsPtzField)     && getLimits(Qn::LogicalPtzCoordinateSpace, &data->logicalLimits))       data->fields |= Qn::LogicalLimitsPtzField;
    if((query & Qn::FlipPtzField)              && getFlip(&data->flip))                                                 data->fields |= Qn::FlipPtzField;
    if((query & Qn::PresetsPtzField)           && getPresets(&data->presets))                                           data->fields |= Qn::PresetsPtzField;
    if((query & Qn::ToursPtzField)             && getTours(&data->tours))                                               data->fields |= Qn::ToursPtzField;
}

