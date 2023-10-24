// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <array>

#include <nx/reflect/enum_instrument.h>

namespace nx::vms::common {
namespace ptz {

enum class Command
{
    /**%apidoc
     * %caption InvalidPtzCommand
     */
    invalid = -1,

    /**%apidoc
     * %caption ContinuousMovePtzCommand
     */
    continuousMove,

    /**%apidoc
     * %caption ContinuousFocusPtzCommand
     */
    continuousFocus,

    /**%apidoc
     * %caption AbsoluteDeviceMovePtzCommand
     */
    absoluteDeviceMove,

    /**%apidoc
     * %caption AbsoluteLogicalMovePtzCommand
     */
    absoluteLogicalMove,

    /**%apidoc
     * %caption ViewportMovePtzCommand
     */
    viewportMove,

    /**%apidoc
     * %caption GetDevicePositionPtzCommand
     */
    getDevicePosition,

    /**%apidoc
     * %caption GetLogicalPositionPtzCommand
     */
    getLogicalPosition,

    /**%apidoc
     * %caption GetDeviceLimitsPtzCommand
     */
    getDeviceLimits,

    /**%apidoc
     * %caption GetLogicalLimitsPtzCommand
     */
    getLogicalLimits,

    /**%apidoc
     * %caption GetFlipPtzCommand
     */
    getFlip,

    /**%apidoc
     * %caption CreatePresetPtzCommand
     */
    createPreset,

    /**%apidoc
     * %caption UpdatePresetPtzCommand
     */
    updatePreset,

    /**%apidoc
     * %caption RemovePresetPtzCommand
     */
    removePreset,

    /**%apidoc
     * %caption ActivatePresetPtzCommand
     */
    activatePreset,

    /**%apidoc
     * %caption GetPresetsPtzCommand
     */
    getPresets,

    /**%apidoc
     * %caption CreateTourPtzCommand
     */
    createTour,

    /**%apidoc
     * %caption RemoveTourPtzCommand
     */
    removeTour,

    /**%apidoc
     * %caption ActivateTourPtzCommand
     */
    activateTour,

    /**%apidoc
     * %caption GetToursPtzCommand
     */
    getTours,

    /**%apidoc
     * %caption GetActiveObjectPtzCommand
     */
    getActiveObject,

    /**%apidoc
     * %caption UpdateHomeObjectPtzCommand
     */
    updateHomeObject,

    /**%apidoc
     * %caption GetHomeObjectPtzCommand
     */
    getHomeObject,

    /**%apidoc
     * %caption GetAuxiliaryTraitsPtzCommand
     */
    getAuxiliaryTraits,

    /**%apidoc
     * %caption RunAuxiliaryCommandPtzCommand
     */
    runAuxiliaryCommand,

    /**%apidoc
     * %caption GetDataPtzCommand
     */
    getData,

    /**%apidoc
     * %caption RelativeMovePtzCommand
     */
    relativeMove,

    /**%apidoc
     * %caption RelativeFocusPtzCommand
     */
    relativeFocus,
};

inline size_t qHash(const Command& command, uint seed)
{
    return ::qHash((int)command, seed);
}

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(Command*, Visitor&& visitor)
{
    // ATTENTION: Assigning backwards-compatible misspelled values with `Auxiliary` in them.
    // Such values should go before the correct ones, so both versions are supported on input,
    // and only deprecated misspelled version is supported on output.
    using Item = nx::reflect::enumeration::Item<Command>;
    return visitor(
        Item{Command::continuousMove, "ContinuousMovePtzCommand"},
        Item{Command::continuousFocus, "ContinuousFocusPtzCommand"},
        Item{Command::absoluteDeviceMove, "AbsoluteDeviceMovePtzCommand"},
        Item{Command::absoluteLogicalMove, "AbsoluteLogicalMovePtzCommand"},
        Item{Command::viewportMove, "ViewportMovePtzCommand"},
        Item{Command::getDevicePosition, "GetDevicePositionPtzCommand"},
        Item{Command::getLogicalPosition, "GetLogicalPositionPtzCommand"},
        Item{Command::getDeviceLimits, "GetDeviceLimitsPtzCommand"},
        Item{Command::getLogicalLimits, "GetLogicalLimitsPtzCommand"},
        Item{Command::getFlip, "GetFlipPtzCommand"},
        Item{Command::createPreset, "CreatePresetPtzCommand"},
        Item{Command::updatePreset, "UpdatePresetPtzCommand"},
        Item{Command::removePreset, "RemovePresetPtzCommand"},
        Item{Command::activatePreset, "ActivatePresetPtzCommand"},
        Item{Command::getPresets, "GetPresetsPtzCommand"},
        Item{Command::createTour, "CreateTourPtzCommand"},
        Item{Command::removeTour, "RemoveTourPtzCommand"},
        Item{Command::activateTour, "ActivateTourPtzCommand"},
        Item{Command::getTours, "GetToursPtzCommand"},
        Item{Command::getActiveObject, "GetActiveObjectPtzCommand"},
        Item{Command::updateHomeObject, "UpdateHomeObjectPtzCommand"},
        Item{Command::getHomeObject, "GetHomeObjectPtzCommand"},
        // Intentional typo.
        Item{Command::getAuxiliaryTraits, "GetAuxilaryTraitsPtzCommand"},
        // For deserialization.
        Item{Command::getAuxiliaryTraits, "GetAuxiliaryTraitsPtzCommand"},
        // Intentional typo.
        Item{Command::runAuxiliaryCommand, "RunAuxilaryCommandPtzCommand"},
        // For deserialization.
        Item{Command::runAuxiliaryCommand, "RunAuxiliaryCommandPtzCommand"},
        Item{Command::getData, "GetDataPtzCommand"},
        Item{Command::relativeMove, "RelativeMovePtzCommand"},
        Item{Command::relativeFocus, "RelativeFocusPtzCommand"},
        Item{Command::invalid, "InvalidPtzCommand"}
    );
}

} // namespace ptz
} // namespace nx::vms::common
