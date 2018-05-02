#pragma once

#include <QtCore/QMetaType>

#include <nx/fusion/model_functions_fwd.h>

struct Ptz
{
    Q_GADGET
    Q_ENUMS(Capability)
    Q_ENUMS(Trait)

    Q_FLAGS(Capabilities)
    Q_FLAGS(Traits)

public:
    enum Capability
    {
        NoPtzCapabilities = 0x00000000,

        ContinuousPanCapability = 0x00000001,
        ContinuousTiltCapability = 0x00000002,
        ContinuousZoomCapability = 0x00000004,
        ContinuousFocusCapability = 0x00000008,

        AbsolutePanCapability = 0x00000010,
        AbsoluteTiltCapability = 0x00000020,
        AbsoluteZoomCapability = 0x00000040,

        ViewportPtzCapability = 0x00000080,

        FlipPtzCapability = 0x00000100,
        LimitsPtzCapability = 0x00000200,

        DevicePositioningPtzCapability = 0x00001000,
        LogicalPositioningPtzCapability = 0x00002000,

        PresetsPtzCapability = 0x00010000,
        ToursPtzCapability = 0x00020000,
        ActivityPtzCapability = 0x00040000,
        HomePtzCapability = 0x00080000,

        AsynchronousPtzCapability = 0x00100000,
        SynchronizedPtzCapability = 0x00200000,
        VirtualPtzCapability = 0x00400000,

        AuxilaryPtzCapability = 0x01000000,

        NativePresetsPtzCapability = 0x08000000,
        NoNxPresetsPtzCapability = 0x10000000, //<< TODO: remove it. Introduce NativePresets and NxPresets capabilities instead

        ContinuousPanTiltCapabilities =
            ContinuousPanCapability | ContinuousTiltCapability,

        ContinuousPtzCapabilities =
            ContinuousPanCapability | ContinuousTiltCapability | ContinuousZoomCapability,

        AbsolutePtzCapabilities =
            AbsolutePanCapability | AbsoluteTiltCapability | AbsoluteZoomCapability,
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)

    enum Trait
    {
        NoPtzTraits = 0x00,
        FourWayPtzTrait = 0x01,
        EightWayPtzTrait = 0x02,
        ManualAutoFocusPtzTrait = 0x04,
    };
    Q_DECLARE_FLAGS(Traits, Trait)
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Ptz::Traits)
Q_DECLARE_OPERATORS_FOR_FLAGS(Ptz::Capabilities)

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Ptz::Trait)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Ptz::Capability)

QN_FUSION_DECLARE_FUNCTIONS(Ptz::Trait, (metatype)(numeric)(lexical))
QN_FUSION_DECLARE_FUNCTIONS(Ptz::Traits, (metatype)(numeric)(lexical))
QN_FUSION_DECLARE_FUNCTIONS(Ptz::Capabilities, (metatype)(numeric)(lexical))


