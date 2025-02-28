// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/utils/json/flags.h>
#include <nx/vms/api/types/ptz_types.h>

struct NX_VMS_COMMON_API Ptz
{
    Q_GADGET
    Q_ENUMS(Trait)

    Q_FLAGS(Traits)

public:
    using Capability = nx::vms::api::ptz::Capability;
    using Capabilities = nx::vms::api::ptz::Capabilities;
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

    static constexpr qreal kMinimumSensitivity = nx::vms::api::ptz::kMinimumSensitivity;
    static constexpr qreal kMaximumSensitivity = nx::vms::api::ptz::kMaximumSensitivity;
    static constexpr qreal kDefaultSensitivity = nx::vms::api::ptz::kDefaultSensitivity;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Ptz::Traits)
