// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMetaType>
#include <QtCore/QObject>

#include <nx/reflect/enum_instrument.h>
#include <nx/utils/json/flags.h>

namespace nx::vms::api::ptz {
Q_NAMESPACE_EXPORT(NX_VMS_API)
Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")

enum class Capability
{
    none = 0x00000000,

    continuousPan = 0x00000001,
    continuousTilt = 0x00000002,
    continuousZoom = 0x00000004,
    continuousFocus = 0x00000008,
    continuousRotation = 0x20000000,

    absolutePan = 0x00000010,
    absoluteTilt = 0x00000020,
    absoluteZoom = 0x00000040,
    absoluteRotation = 0x40000000,

    relativePan = 0x00000400,
    relativeTilt = 0x00000800,
    relativeZoom = 0x00004000,
    relativeRotation = 0x00008000,
    relativeFocus = 0x00800000,

    viewport = 0x00000080,

    flip = 0x00000100,
    limits = 0x00000200,

    devicePositioning = 0x00001000,
    logicalPositioning = 0x00002000,

    presets = 0x00010000,
    tours = 0x00020000,
    activity = 0x00040000,
    home = 0x00080000,

    asynchronous = 0x00100000,
    synchronized = 0x00200000,
    virtual_ = 0x00400000,

    auxiliary = 0x01000000,

    nativePresets = 0x08000000,
    disabledNxPresets = 0x10000000,

    /**%apidoc[unused] */
    positioningDeviceLogical =
        devicePositioning | logicalPositioning,

    /**%apidoc[unused] */
    absolutePanTilt =
        absolutePan | absoluteTilt,

    /**%apidoc[unused] */
    absolutePanTiltZoom =
        absolutePanTilt | absoluteZoom,

    /**%apidoc[unused] */
    absolutePanTiltRotation =
        absolutePanTilt | absoluteRotation,

    /**%apidoc[unused] */
    absolutePanTiltZoomRotation =
        absolutePanTiltRotation | absoluteZoom,

    /**%apidoc[unused] */
    continuousPanTilt =
        continuousPan | continuousTilt,

    /**%apidoc[unused] */
    continuousPanTiltZoom =
        continuousPanTilt | continuousZoom,

    /**%apidoc[unused] */
    continuousPanTiltRotation =
        continuousPanTilt | continuousRotation,

    /**%apidoc[unused] */
    continuousPanTiltZoomRotation =
        continuousPanTiltRotation | continuousZoom,

    /**%apidoc[unused] */
    relativePanTilt =
        relativePan | relativeTilt,

    /**%apidoc[unused] */
    relativePanTiltZoom =
        relativePanTilt | relativeZoom,

    /**%apidoc[unused] */
    relativePanTiltRotation =
        relativePanTilt | relativeRotation,

    /**%apidoc[unused] */
    relativePanTiltZoomRotation =
        relativePanTiltRotation | relativeZoom
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(Capability*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<Capability>;
    return visitor(
        Item{Capability::none, "none"},
        Item{Capability::none, "NoPtzCapabilities"},
        Item{Capability::continuousPan, "continuousPan"},
        Item{Capability::continuousPan, "ContinuousPanCapability"},
        Item{Capability::continuousTilt, "continuousTilt"},
        Item{Capability::continuousTilt, "ContinuousTiltCapability"},
        Item{Capability::continuousZoom, "continuousZoom"},
        Item{Capability::continuousZoom, "ContinuousZoomCapability"},
        Item{Capability::continuousFocus, "continuousFocus"},
        Item{Capability::continuousFocus, "ContinuousFocusCapability"},
        Item{Capability::continuousRotation, "continuousRotation"},
        Item{Capability::continuousRotation, "ContinuousRotationCapability"},
        Item{Capability::absolutePan, "absolutePan"},
        Item{Capability::absolutePan, "AbsolutePanCapability"},
        Item{Capability::absoluteTilt, "absoluteTilt"},
        Item{Capability::absoluteTilt, "AbsoluteTiltCapability"},
        Item{Capability::absoluteZoom, "absoluteZoom"},
        Item{Capability::absoluteZoom, "AbsoluteZoomCapability"},
        Item{Capability::absoluteRotation, "absoluteRotation"},
        Item{Capability::absoluteRotation, "AbsoluteRotationCapability"},
        Item{Capability::relativePan, "relativePan"},
        Item{Capability::relativePan, "RelativePanCapability"},
        Item{Capability::relativeTilt, "relativeTilt"},
        Item{Capability::relativeTilt, "RelativeTiltCapability"},
        Item{Capability::relativeZoom, "relativeZoom"},
        Item{Capability::relativeZoom, "RelativeZoomCapability"},
        Item{Capability::relativeRotation, "relativeRotation"},
        Item{Capability::relativeRotation, "RelativeRotationCapability"},
        Item{Capability::relativeFocus, "relativeFocus"},
        Item{Capability::relativeFocus, "RelativeFocusCapability"},
        Item{Capability::viewport, "viewport"},
        Item{Capability::viewport, "ViewportPtzCapability"},
        Item{Capability::flip, "flip"},
        Item{Capability::flip, "FlipPtzCapability"},
        Item{Capability::limits, "limits"},
        Item{Capability::limits, "LimitsPtzCapability"},
        Item{Capability::devicePositioning, "devicePositioning"},
        Item{Capability::devicePositioning, "DevicePositioningPtzCapability"},
        Item{Capability::logicalPositioning, "logicalPositioning"},
        Item{Capability::logicalPositioning, "LogicalPositioningPtzCapability"},
        Item{Capability::presets, "presets"},
        Item{Capability::presets, "PresetsPtzCapability"},
        Item{Capability::tours, "tours"},
        Item{Capability::tours, "ToursPtzCapability"},
        Item{Capability::activity, "activity"},
        Item{Capability::activity, "ActivityPtzCapability"},
        Item{Capability::home, "home"},
        Item{Capability::home, "HomePtzCapability"},
        Item{Capability::asynchronous, "asynchronous"},
        Item{Capability::asynchronous, "AsynchronousPtzCapability"},
        Item{Capability::synchronized, "synchronized"},
        Item{Capability::synchronized, "SynchronizedPtzCapability"},
        Item{Capability::virtual_, "virtual"},
        Item{Capability::virtual_, "VirtualPtzCapability"},
        Item{Capability::auxiliary, "auxiliary"},
        Item{Capability::auxiliary, "AuxiliaryPtzCapability"},
        Item{Capability::nativePresets, "nativePresets"},
        Item{Capability::nativePresets, "NativePresetsPtzCapability"},
        Item{Capability::disabledNxPresets, "disabledNxPresets"},
        Item{Capability::disabledNxPresets, "NoNxPresetsPtzCapability"}
    );
}

Q_DECLARE_FLAGS(Capabilities, Capability)
Q_ENUM_NS(Capability)
Q_FLAG_NS(Capabilities)
Q_DECLARE_OPERATORS_FOR_FLAGS(Capabilities)

NX_REFLECTION_ENUM_CLASS(PresetType,
    undefined,
    system,
    native
)

static constexpr qreal kMinimumSensitivity = 0.1;
static constexpr qreal kMaximumSensitivity = 1.0;
static constexpr qreal kDefaultSensitivity = 1.0;

} // namespace nx::vms::api::ptz
