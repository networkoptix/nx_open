#include "abstract_ptz_controller.h"

#include <cassert>

#include <core/resource/resource.h>

QnAbstractPtzController::QnAbstractPtzController(const QnResourcePtr &resource): 
    m_resource(resource) 
{}

QnAbstractPtzController::~QnAbstractPtzController() {
    return;
}

bool QnAbstractPtzController::getData(Qn::PtzDataFields query, QnPtzData *data) {
    data->query = query;
    data->fields = Qn::NoPtzFields;
    data->capabilities = getCapabilities();
    
    if((query & Qn::CapabilitiesPtzField))       { data->capabilities = getCapabilities();                              data->fields |= Qn::CapabilitiesPtzField; }
    if((query & Qn::DevicePositionPtzField)     && getPosition(Qn::DevicePtzCoordinateSpace, &data->devicePosition))    data->fields |= Qn::DevicePositionPtzField;
    if((query & Qn::LogicalPositionPtzField)    && getPosition(Qn::LogicalPtzCoordinateSpace, &data->logicalPosition))  data->fields |= Qn::LogicalPositionPtzField;
    if((query & Qn::DeviceLimitsPtzField)       && getLimits(Qn::DevicePtzCoordinateSpace, &data->deviceLimits))        data->fields |= Qn::DeviceLimitsPtzField;
    if((query & Qn::LogicalLimitsPtzField)      && getLimits(Qn::LogicalPtzCoordinateSpace, &data->logicalLimits))      data->fields |= Qn::LogicalLimitsPtzField;
    if((query & Qn::FlipPtzField)               && getFlip(&data->flip))                                                data->fields |= Qn::FlipPtzField;
    if((query & Qn::PresetsPtzField)            && getPresets(&data->presets))                                          data->fields |= Qn::PresetsPtzField;
    if((query & Qn::ToursPtzField)              && getTours(&data->tours))                                              data->fields |= Qn::ToursPtzField;
    if((query & Qn::ActiveObjectPtzField)       && getActiveObject(&data->activeObject))                                data->fields |= Qn::ActiveObjectPtzField;
    if((query & Qn::HomeObjectPtzField)         && getHomeObject(&data->homeObject))                                    data->fields |= Qn::HomeObjectPtzField;

    return true;
}

bool QnAbstractPtzController::supports(Qn::PtzCommand command) {
    Qn::PtzCapabilities capabilities = getCapabilities();

    switch (command) {
    case Qn::ContinuousMovePtzCommand:       
        return (capabilities & Qn::ContinuousPtzCapabilities);

    case Qn::GetDevicePositionPtzCommand:
    case Qn::AbsoluteDeviceMovePtzCommand:
        return (capabilities & Qn::AbsolutePtzCapabilities) && (capabilities & Qn::DevicePositioningPtzCapability);

    case Qn::GetLogicalPositionPtzCommand:
    case Qn::AbsoluteLogicalMovePtzCommand:        
        return (capabilities & Qn::AbsolutePtzCapabilities) && (capabilities & Qn::LogicalPositioningPtzCapability);

    case Qn::ViewportMovePtzCommand:        
        return (capabilities & Qn::ViewportPtzCapability);

    case Qn::GetDeviceLimitsPtzCommand:           
        return (capabilities & Qn::LimitsPtzCapability) && (capabilities & Qn::DevicePositioningPtzCapability);

    case Qn::GetLogicalLimitsPtzCommand:           
        return (capabilities & Qn::LimitsPtzCapability) && (capabilities & Qn::LogicalPositioningPtzCapability);

    case Qn::GetFlipPtzCommand:             
        return (capabilities & Qn::FlipPtzCapability);

    case Qn::CreatePresetPtzCommand:
    case Qn::UpdatePresetPtzCommand:
    case Qn::RemovePresetPtzCommand:
    case Qn::ActivatePresetPtzCommand:
    case Qn::GetPresetsPtzCommand:          
        return (capabilities & Qn::PresetsPtzCapability);

    case Qn::CreateTourPtzCommand:
    case Qn::RemoveTourPtzCommand:
    case Qn::ActivateTourPtzCommand:
    case Qn::GetToursPtzCommand:            
        return (capabilities & Qn::ToursPtzCapability);

    case Qn::UpdateHomeObjectPtzCommand:
    case Qn::GetHomeObjectPtzCommand:
        return (capabilities & Qn::HomePtzCapability);

    case Qn::GetDataPtzCommand:
        return true;

    default:                                
        assert(false); 
        return false; /* We should never get here. */
    }
}

Qn::PtzCommand QnAbstractPtzController::spaceCommand(Qn::PtzCommand command, Qn::PtzCoordinateSpace space) {
    switch (command) {
    case Qn::AbsoluteDeviceMovePtzCommand:
    case Qn::AbsoluteLogicalMovePtzCommand:
        return space == Qn::DevicePtzCoordinateSpace ? Qn::AbsoluteDeviceMovePtzCommand : Qn::AbsoluteLogicalMovePtzCommand;
    case Qn::GetDevicePositionPtzCommand:
    case Qn::GetLogicalPositionPtzCommand:
        return space == Qn::DevicePtzCoordinateSpace ? Qn::GetDevicePositionPtzCommand : Qn::GetLogicalPositionPtzCommand;
    case Qn::GetDeviceLimitsPtzCommand:
    case Qn::GetLogicalLimitsPtzCommand:
        return space == Qn::DevicePtzCoordinateSpace ? Qn::GetDeviceLimitsPtzCommand : Qn::GetLogicalLimitsPtzCommand;
    default:
        return command;
    }
}

