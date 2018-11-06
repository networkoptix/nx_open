#pragma once

#include <core/ptz/ptz_constants.h>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/serialization/qt_enums.h>
#include <nx/utils/std/optional.h>
#include <nx/core/ptz/type.h>

namespace nx {
namespace core {
namespace ptz {

struct OverridePart
{
    // Intentionally not serializable since fusion doesn't have a way to properly
    // handle std::optional/boost::optional fields.
    std::optional<Ptz::Capabilities> capabilitiesOverride;

    Ptz::Capabilities capabilitiesToAdd;
    Ptz::Capabilities capabilitiesToRemove;

    Qt::Orientations flip;
    Ptz::Traits traits;
};
#define PtzOverridePart_Fields (capabilitiesToAdd)\
    (capabilitiesToRemove)\
    (flip)\
    (traits)

QN_FUSION_DECLARE_FUNCTIONS(OverridePart, (json))

struct Override
{
    static const QString kPtzOverrideKey;

    // This struct exists only because enums are not supported as map keys by fusion.
    OverridePart operational;
    OverridePart configurational;

    OverridePart overrideByType(nx::core::ptz::Type ptzType) const;
};
#define PtzOverride_Fields (operational)(configurational)

QN_FUSION_DECLARE_FUNCTIONS(Override, (json))

} // namespace ptz
} // namespace core
} // namespace nx

Q_DECLARE_METATYPE(nx::core::ptz::OverridePart);
Q_DECLARE_METATYPE(nx::core::ptz::Override);
