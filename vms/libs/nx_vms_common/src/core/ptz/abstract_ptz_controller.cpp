// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "abstract_ptz_controller.h"

#include <core/resource/resource.h>

using namespace nx::core;
using namespace nx::vms::common::ptz;

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

void QnAbstractPtzController::invalidate()
{
}

QnResourcePtr QnAbstractPtzController::resource() const
{
    return m_resource;
}

std::optional<QnPtzTour> QnAbstractPtzController::getActiveTour()
{
    return std::nullopt;
}

bool QnAbstractPtzController::hasCapabilities(
    Ptz::Capabilities capabilities,
    const Options& options) const
{
    return (getCapabilities(options) & capabilities) == capabilities;
}

bool QnAbstractPtzController::getData(
    QnPtzData* data,
    DataFields query,
    const Options& options) const
{
    data->query = query;
    data->fields = DataField::none;

    if (query.testFlag(DataField::capabilities))
    {
        data->capabilities = getCapabilities(options);
        data->fields |= DataField::capabilities;
    }

    if (query.testFlag(DataField::devicePosition)
        && getPosition(&data->devicePosition, CoordinateSpace::device, options))
    {
        data->fields |= DataField::devicePosition;
    }

    if (query.testFlag(DataField::logicalPosition)
        && getPosition(&data->logicalPosition, CoordinateSpace::logical, options))
    {
        data->fields |= DataField::logicalPosition;
    }

    if (query.testFlag(DataField::deviceLimits)
        && getLimits(&data->deviceLimits, CoordinateSpace::device, options))
    {
        data->fields |= DataField::deviceLimits;
    }

    if (query.testFlag(DataField::logicalLimits)
        && getLimits(&data->logicalLimits, CoordinateSpace::logical, options))
    {
        data->fields |= DataField::logicalLimits;
    }

    if (query.testFlag(DataField::flip) && getFlip(&data->flip, options))
    {
        data->fields |= DataField::flip;
    }

    if (query.testFlag(DataField::presets)
        && options.type == Type::operational
        && getPresets(&data->presets))
    {
        data->fields |= DataField::presets;
    }

    if (query.testFlag(DataField::tours)
        && options.type == Type::operational
        && getTours(&data->tours))
    {
        data->fields |= DataField::tours;
    }

    if (query.testFlag(DataField::activeObject)
        && options.type == Type::operational
        && getActiveObject(&data->activeObject))
    {
        data->fields |= DataField::activeObject;
    }

    if (query.testFlag(DataField::homeObject)
        && options.type == Type::operational
        && getHomeObject(&data->homeObject))
    {
        data->fields |= DataField::homeObject;
    }

    if (query.testFlag(DataField::auxiliaryTraits)
        && getAuxiliaryTraits(&data->auxiliaryTraits, options))
    {
        data->fields |= DataField::auxiliaryTraits;
    }

    return true;
}

bool QnAbstractPtzController::supports(
    Command command,
    const Options& options) const
{
    const auto capabilities = getCapabilities(options);
    switch (command)
    {
        case Command::continuousMove:
            return (capabilities & Ptz::ContinuousPtzCapabilities);

        case Command::continuousFocus:
            return capabilities.testFlag(Ptz::ContinuousFocusCapability);

        case Command::getDevicePosition:
        case Command::absoluteDeviceMove:
            return (capabilities & Ptz::AbsolutePtzCapabilities)
                && capabilities.testFlag(Ptz::DevicePositioningPtzCapability);

        case Command::getLogicalPosition:
        case Command::absoluteLogicalMove:
            return (capabilities & Ptz::AbsolutePtzCapabilities)
                && capabilities.testFlag(Ptz::LogicalPositioningPtzCapability);

        case Command::viewportMove:
            return capabilities.testFlag(Ptz::ViewportPtzCapability);

        case Command::getDeviceLimits:
            return capabilities.testFlag(Ptz::LimitsPtzCapability)
                && capabilities.testFlag(Ptz::DevicePositioningPtzCapability);

        case Command::getLogicalLimits:
            return capabilities.testFlag(Ptz::LimitsPtzCapability)
                && capabilities.testFlag(Ptz::LogicalPositioningPtzCapability);

        case Command::getFlip:
            return capabilities.testFlag(Ptz::FlipPtzCapability);

        case Command::createPreset:
        case Command::updatePreset:
        case Command::removePreset:
        case Command::activatePreset:
        case Command::getPresets:
            return capabilities.testFlag(Ptz::PresetsPtzCapability);

        case Command::getActiveObject:
            return capabilities.testFlag(Ptz::PresetsPtzCapability)
                || capabilities.testFlag(Ptz::ToursPtzCapability);

        case Command::createTour:
        case Command::removeTour:
        case Command::activateTour:
        case Command::getTours:
            return capabilities.testFlag(Ptz::ToursPtzCapability);

        case Command::updateHomeObject:
        case Command::getHomeObject:
            return capabilities.testFlag(Ptz::HomePtzCapability);

        case Command::getAuxiliaryTraits:
        case Command::runAuxiliaryCommand:
            return capabilities.testFlag(Ptz::AuxiliaryPtzCapability);

        case Command::getData:
            return true;

        default:
            NX_ASSERT(false);
            return false; /* We should never get here. */
    }
}

Command QnAbstractPtzController::spaceCommand(
    Command command,
    CoordinateSpace space)
{
    switch (command)
    {
        case Command::absoluteDeviceMove:
        case Command::absoluteLogicalMove:
            return space == CoordinateSpace::device
                ? Command::absoluteDeviceMove
                : Command::absoluteLogicalMove;
        case Command::getDevicePosition:
        case Command::getLogicalPosition:
            return space == CoordinateSpace::device
                ? Command::getDevicePosition
                : Command::getLogicalPosition;
        case Command::getDeviceLimits:
        case Command::getLogicalLimits:
            return space == CoordinateSpace::device
                ? Command::getDeviceLimits
                : Command::getLogicalLimits;
        default:
            return command;
    }
}

bool deserialize(const QString& /*value*/, QnPtzObject* /*target*/)
{
    NX_ASSERT(false, "Not implemented");
    return false;
}
