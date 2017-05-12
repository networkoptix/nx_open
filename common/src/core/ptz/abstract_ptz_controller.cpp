#include "abstract_ptz_controller.h"

#include <core/resource/resource.h>

const qreal QnAbstractPtzController::MaxPtzSpeed = 1.0;

QnAbstractPtzController::QnAbstractPtzController(const QnResourcePtr& resource):
    m_resource(resource)
{
}

QnAbstractPtzController::~QnAbstractPtzController()
{
}

QnResourcePtr QnAbstractPtzController::resource() const
{
    return m_resource;
}

bool QnAbstractPtzController::hasCapabilities(Ptz::Capabilities capabilities) const
{
    return (getCapabilities() & capabilities) == capabilities;
}

bool QnAbstractPtzController::getData(Qn::PtzDataFields query, QnPtzData* data) const
{
    data->query = query;
    data->fields = Qn::NoPtzFields;
    data->capabilities = getCapabilities();

    if ((query & Qn::CapabilitiesPtzField))
    {
        data->capabilities = getCapabilities();
        data->fields |= Qn::CapabilitiesPtzField;
    }

    if ((query & Qn::DevicePositionPtzField)
        && getPosition(Qn::DevicePtzCoordinateSpace, &data->devicePosition))
    {
        data->fields |= Qn::DevicePositionPtzField;
    }

    if ((query & Qn::LogicalPositionPtzField)
        && getPosition(Qn::LogicalPtzCoordinateSpace, &data->logicalPosition))
    {
        data->fields |= Qn::LogicalPositionPtzField;
    }

    if ((query & Qn::DeviceLimitsPtzField)
        && getLimits(Qn::DevicePtzCoordinateSpace, &data->deviceLimits))        \
    {
        data->fields |= Qn::DeviceLimitsPtzField;
    }

    if ((query & Qn::LogicalLimitsPtzField)
        && getLimits(Qn::LogicalPtzCoordinateSpace, &data->logicalLimits))
    {
        data->fields |= Qn::LogicalLimitsPtzField;
    }

    if ((query & Qn::FlipPtzField)
        && getFlip(&data->flip))
    {
        data->fields |= Qn::FlipPtzField;
    }

    if ((query & Qn::PresetsPtzField)
        && getPresets(&data->presets))
    {
        data->fields |= Qn::PresetsPtzField;
    }

    if ((query & Qn::ToursPtzField)
        && getTours(&data->tours))
    {
        data->fields |= Qn::ToursPtzField;
    }

    if ((query & Qn::ActiveObjectPtzField)
        && getActiveObject(&data->activeObject))
    {
        data->fields |= Qn::ActiveObjectPtzField;
    }

    if ((query & Qn::HomeObjectPtzField)
        && getHomeObject(&data->homeObject))
    {
        data->fields |= Qn::HomeObjectPtzField;
    }

    if ((query & Qn::AuxilaryTraitsPtzField)
        && getAuxilaryTraits(&data->auxilaryTraits))
    {
        data->fields |= Qn::AuxilaryTraitsPtzField;
    }

    return true;
}

bool QnAbstractPtzController::supports(Qn::PtzCommand command) const
{
    const Ptz::Capabilities capabilities = getCapabilities();
    switch (command)
    {
        case Qn::ContinuousMovePtzCommand:
            return (capabilities & Ptz::ContinuousPtzCapabilities);

        case Qn::ContinuousFocusPtzCommand:
            return capabilities.testFlag(Ptz::ContinuousFocusCapability);

        case Qn::GetDevicePositionPtzCommand:
        case Qn::AbsoluteDeviceMovePtzCommand:
            return (capabilities & Ptz::AbsolutePtzCapabilities)
                && capabilities.testFlag(Ptz::DevicePositioningPtzCapability);

        case Qn::GetLogicalPositionPtzCommand:
        case Qn::AbsoluteLogicalMovePtzCommand:
            return (capabilities & Ptz::AbsolutePtzCapabilities)
                && capabilities.testFlag(Ptz::LogicalPositioningPtzCapability);

        case Qn::ViewportMovePtzCommand:
            return capabilities.testFlag(Ptz::ViewportPtzCapability);

        case Qn::GetDeviceLimitsPtzCommand:
            return capabilities.testFlag(Ptz::LimitsPtzCapability)
                && capabilities.testFlag(Ptz::DevicePositioningPtzCapability);

        case Qn::GetLogicalLimitsPtzCommand:
            return capabilities.testFlag(Ptz::LimitsPtzCapability)
                && capabilities.testFlag(Ptz::LogicalPositioningPtzCapability);

        case Qn::GetFlipPtzCommand:
            return capabilities.testFlag(Ptz::FlipPtzCapability);

        case Qn::CreatePresetPtzCommand:
        case Qn::UpdatePresetPtzCommand:
        case Qn::RemovePresetPtzCommand:
        case Qn::ActivatePresetPtzCommand:
        case Qn::GetPresetsPtzCommand:
            return capabilities.testFlag(Ptz::PresetsPtzCapability);

        case Qn::GetActiveObjectPtzCommand:
            return capabilities.testFlag(Ptz::PresetsPtzCapability)
                || capabilities.testFlag(Ptz::ToursPtzCapability);

        case Qn::CreateTourPtzCommand:
        case Qn::RemoveTourPtzCommand:
        case Qn::ActivateTourPtzCommand:
        case Qn::GetToursPtzCommand:
            return capabilities.testFlag(Ptz::ToursPtzCapability);

        case Qn::UpdateHomeObjectPtzCommand:
        case Qn::GetHomeObjectPtzCommand:
            return capabilities.testFlag(Ptz::HomePtzCapability);

        case Qn::GetAuxilaryTraitsPtzCommand:
        case Qn::RunAuxilaryCommandPtzCommand:
            return capabilities.testFlag(Ptz::AuxilaryPtzCapability);

        case Qn::GetDataPtzCommand:
            return true;

        default:
            NX_ASSERT(false);
            return false; /* We should never get here. */
    }
}

Qn::PtzCommand QnAbstractPtzController::spaceCommand(
    Qn::PtzCommand command,
    Qn::PtzCoordinateSpace space)
{
    switch (command)
    {
        case Qn::AbsoluteDeviceMovePtzCommand:
        case Qn::AbsoluteLogicalMovePtzCommand:
            return space == Qn::DevicePtzCoordinateSpace
                ? Qn::AbsoluteDeviceMovePtzCommand
                : Qn::AbsoluteLogicalMovePtzCommand;
        case Qn::GetDevicePositionPtzCommand:
        case Qn::GetLogicalPositionPtzCommand:
            return space == Qn::DevicePtzCoordinateSpace
                ? Qn::GetDevicePositionPtzCommand
                : Qn::GetLogicalPositionPtzCommand;
        case Qn::GetDeviceLimitsPtzCommand:
        case Qn::GetLogicalLimitsPtzCommand:
            return space == Qn::DevicePtzCoordinateSpace
                ? Qn::GetDeviceLimitsPtzCommand
                : Qn::GetLogicalLimitsPtzCommand;
        default:
            return command;
    }
}

bool deserialize(const QString& /*value*/, QnPtzObject* /*target*/)
{
    Q_ASSERT_X(0, Q_FUNC_INFO, "Not implemented");
    return false;
}
