#include "abstract_ptz_controller.h"

#include <core/resource/resource.h>

using namespace nx::core;

const qreal QnAbstractPtzController::MaxPtzSpeed = 1.0;

QnAbstractPtzController::QnAbstractPtzController(const QnResourcePtr& resource):
    m_resource(resource)
{
}

QnAbstractPtzController::~QnAbstractPtzController()
{
}

void QnAbstractPtzController::initialize()
{
}

QnResourcePtr QnAbstractPtzController::resource() const
{
    return m_resource;
}

bool QnAbstractPtzController::hasCapabilities(
    Ptz::Capabilities capabilities,
    const nx::core::ptz::Options& options) const
{
    return (getCapabilities(options) & capabilities) == capabilities;
}

bool QnAbstractPtzController::getData(
    Qn::PtzDataFields query,
    QnPtzData* data,
    const nx::core::ptz::Options& options) const
{
    data->query = query;
    data->fields = Qn::NoPtzFields;

    if (query.testFlag(Qn::CapabilitiesPtzField))
    {
        data->capabilities = getCapabilities(options);
        data->fields |= Qn::CapabilitiesPtzField;
    }

    if (query.testFlag(Qn::DevicePositionPtzField)
        && getPosition(Qn::DevicePtzCoordinateSpace, &data->devicePosition, options))
    {
        data->fields |= Qn::DevicePositionPtzField;
    }

    if (query.testFlag(Qn::LogicalPositionPtzField)
        && getPosition(Qn::LogicalPtzCoordinateSpace, &data->logicalPosition, options))
    {
        data->fields |= Qn::LogicalPositionPtzField;
    }

    if (query.testFlag(Qn::DeviceLimitsPtzField)
        && getLimits(Qn::DevicePtzCoordinateSpace, &data->deviceLimits, options))
    {
        data->fields |= Qn::DeviceLimitsPtzField;
    }

    if (query.testFlag(Qn::LogicalLimitsPtzField)
        && getLimits(Qn::LogicalPtzCoordinateSpace, &data->logicalLimits, options))
    {
        data->fields |= Qn::LogicalLimitsPtzField;
    }

    if (query.testFlag(Qn::FlipPtzField) && getFlip(&data->flip, options))
    {
        data->fields |= Qn::FlipPtzField;
    }

    if (query.testFlag(Qn::PresetsPtzField)
        && options.type == ptz::Type::operational
        && getPresets(&data->presets))
    {
        data->fields |= Qn::PresetsPtzField;
    }

    if (query.testFlag(Qn::ToursPtzField)
        && options.type == ptz::Type::operational
        && getTours(&data->tours))
    {
        data->fields |= Qn::ToursPtzField;
    }

    if (query.testFlag(Qn::ActiveObjectPtzField)
        && options.type == ptz::Type::operational
        && getActiveObject(&data->activeObject))
    {
        data->fields |= Qn::ActiveObjectPtzField;
    }

    if (query.testFlag(Qn::HomeObjectPtzField)
        && options.type == ptz::Type::operational
        && getHomeObject(&data->homeObject))
    {
        data->fields |= Qn::HomeObjectPtzField;
    }

    if (query.testFlag(Qn::AuxiliaryTraitsPtzField)
        && getAuxiliaryTraits(&data->auxiliaryTraits, options))
    {
        data->fields |= Qn::AuxiliaryTraitsPtzField;
    }

    return true;
}

bool QnAbstractPtzController::supports(
    Qn::PtzCommand command,
    const nx::core::ptz::Options& options) const
{
    const auto capabilities = getCapabilities(options);
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

        case Qn::GetAuxiliaryTraitsPtzCommand:
        case Qn::RunAuxiliaryCommandPtzCommand:
            return capabilities.testFlag(Ptz::AuxiliaryPtzCapability);

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
    NX_ASSERT(false, "Not implemented");
    return false;
}
