#include "abstract_ptz_controller.h"

#include <cassert>

#include <core/resource/resource.h>

namespace {
    bool hasSpaceCapabilities(Qn::PtzCapabilities capabilities, Qn::PtzCoordinateSpace space) {
        switch(space) {
        case Qn::DevicePtzCoordinateSpace:  return capabilities & Qn::DevicePositioningPtzCapability;
        case Qn::LogicalPtzCoordinateSpace: return capabilities & Qn::LogicalPositioningPtzCapability;
        default:                            return capabilities & (Qn::DevicePositioningPtzCapability | Qn::LogicalPositioningPtzCapability); 
        }
    }
} // anonymous namespace

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

bool QnAbstractPtzController::supports(Qn::PtzCommand command) {
    return supports(command, static_cast<Qn::PtzCoordinateSpace>(-1));
}

bool QnAbstractPtzController::supports(Qn::PtzCommand command, Qn::PtzCoordinateSpace space) {
    Qn::PtzCapabilities capabilities = getCapabilities();

    switch (command) {
    case Qn::ContinousMovePtzCommand:       
        return (capabilities & Qn::ContinuousPtzCapabilities);

    case Qn::GetPositionPtzCommand:         
    case Qn::AbsoluteMovePtzCommand:        
        return (capabilities & Qn::AbsolutePtzCapabilities) && hasSpaceCapabilities(capabilities, space);

    case Qn::ViewportMovePtzCommand:        
        return (capabilities & Qn::ViewportPtzCapability);

    case Qn::GetLimitsPtzCommand:           
        return (capabilities & Qn::LimitsPtzCapability) && hasSpaceCapabilities(capabilities, space);

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

    case Qn::GetDataPtzCommand:
    case Qn::SynchronizePtzCommand:         
        return true;

    default:                                
        assert(false); 
        return false; /* We should never get here. */
    }
}
