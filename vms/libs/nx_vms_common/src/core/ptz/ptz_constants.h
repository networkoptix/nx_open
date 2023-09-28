// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/serialization/flags.h>

struct NX_VMS_COMMON_API Ptz
{
    Q_GADGET
    Q_ENUMS(Capability)
    Q_ENUMS(Trait)

    Q_FLAGS(Capabilities)
    Q_FLAGS(Traits)

public:
    NX_REFLECTION_ENUM_IN_CLASS(Capability,
        NoPtzCapabilities = 0x00000000,

        ContinuousPanCapability = 0x00000001,
        ContinuousTiltCapability = 0x00000002,
        ContinuousZoomCapability = 0x00000004,
        ContinuousFocusCapability = 0x00000008,
        ContinuousRotationCapability = 0x20000000,

        AbsolutePanCapability = 0x00000010,
        AbsoluteTiltCapability = 0x00000020,
        AbsoluteZoomCapability = 0x00000040,
        AbsoluteRotationCapability = 0x40000000,

        RelativePanCapability = 0x00000400,
        RelativeTiltCapability = 0x00000800,
        RelativeZoomCapability = 0x00004000,
        RelativeRotationCapability = 0x00008000,
        RelativeFocusCapability = 0x00800000,

        ViewportPtzCapability = 0x00000080,

        FlipPtzCapability = 0x00000100,
        LimitsPtzCapability = 0x00000200,

        DevicePositioningPtzCapability = 0x00001000,
        LogicalPositioningPtzCapability = 0x00002000,

        PositioningPtzCapabilities =
            DevicePositioningPtzCapability | LogicalPositioningPtzCapability,

        PresetsPtzCapability = 0x00010000,
        ToursPtzCapability = 0x00020000,
        ActivityPtzCapability = 0x00040000,
        HomePtzCapability = 0x00080000,

        AsynchronousPtzCapability = 0x00100000,
        SynchronizedPtzCapability = 0x00200000,
        VirtualPtzCapability = 0x00400000,

        AuxiliaryPtzCapability = 0x01000000,

        NativePresetsPtzCapability = 0x08000000,
        NoNxPresetsPtzCapability = 0x10000000, //<< TODO: remove it. Introduce NativePresets and NxPresets capabilities instead

        AbsolutePanTiltCapabilities =
            AbsolutePanCapability | AbsoluteTiltCapability,

        AbsolutePtzCapabilities =
            AbsolutePanTiltCapabilities | AbsoluteZoomCapability,

        AbsolutePtrCapabilities =
            AbsolutePanTiltCapabilities | AbsoluteRotationCapability,

        AbsolutePtrzCapabilities =
            AbsolutePtrCapabilities | AbsoluteZoomCapability,

        ContinuousPanTiltCapabilities =
            ContinuousPanCapability | ContinuousTiltCapability,

        ContinuousPtzCapabilities =
            ContinuousPanTiltCapabilities | ContinuousZoomCapability,

        ContinuousPtrCapabilities =
            ContinuousPanTiltCapabilities | ContinuousRotationCapability,

        ContinuousPtrzCapabilities =
            ContinuousPtrCapabilities | ContinuousZoomCapability,

        RelativePanTiltCapabilities =
            RelativePanCapability | RelativeTiltCapability,

        RelativePtzCapabilities =
            RelativePanTiltCapabilities | RelativeZoomCapability,

        RelativePtrCapabilities =
            RelativePanTiltCapabilities | RelativeRotationCapability,

        RelativePtrzCapabilities =
            RelativePtrCapabilities | RelativeZoomCapability
    )

    Q_DECLARE_FLAGS(Capabilities, Capability)

    enum Trait
    {
        NoPtzTraits = 0x00,
        FourWayPtzTrait = 0x01,
        EightWayPtzTrait = 0x02,
        ManualAutoFocusPtzTrait = 0x04,
    };

    template<typename Visitor>
    friend constexpr auto nxReflectVisitAllEnumItems(Trait*, Visitor&& visitor)
    {
        using Item = nx::reflect::enumeration::Item<Trait>;
        return visitor(
            Item{FourWayPtzTrait, "FourWayPtz"},
            Item{EightWayPtzTrait, "EightWayPtz"},
            Item{ManualAutoFocusPtzTrait, "ManualAutoFocus"}
        );
    }

    Q_DECLARE_FLAGS(Traits, Trait)

    static constexpr qreal kMinimumSensitivity = 0.1;
    static constexpr qreal kMaximumSensitivity = 1.0;
    static constexpr qreal kDefaultSensitivity = 1.0;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Ptz::Traits)
Q_DECLARE_OPERATORS_FOR_FLAGS(Ptz::Capabilities)
