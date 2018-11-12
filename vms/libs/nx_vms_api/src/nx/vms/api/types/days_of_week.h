#pragma once

#include <algorithm>

#include <QtCore/QList>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace vms {
namespace api {

enum class DayOfWeek
{
    none = 0,

    monday    = 1 << Qt::Monday,
    tuesday   = 1 << Qt::Tuesday,
    wednesday = 1 << Qt::Wednesday,
    thursday  = 1 << Qt::Thursday,
    friday    = 1 << Qt::Friday,
    saturday  = 1 << Qt::Saturday,
    sunday    = 1 << Qt::Sunday,

    weekdays = monday | tuesday | wednesday | thursday | friday,
    weekends = saturday | sunday,
    all = weekdays | weekends
};
Q_DECLARE_FLAGS(DaysOfWeek, DayOfWeek)
Q_DECLARE_OPERATORS_FOR_FLAGS(DaysOfWeek)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(DayOfWeek)

inline DayOfWeek dayOfWeek(Qt::DayOfWeek day)
{
    return DayOfWeek(1 << day);
}

/* Version for std::set is not supported by our current clang. */
/*
template<template <typename...> class Container, typename... ExtraArgs>
inline DaysOfWeek daysOfWeek(const Container<Qt::DayOfWeek, ExtraArgs...>& days)
{
    return std::accumulate(days.cbegin(), days.cend(), DaysOfWeek(),
        [](DaysOfWeek a, Qt::DayOfWeek b) -> DaysOfWeek
        {
            return a | dayOfWeek(b);
        });
}
*/

template<template <typename> class Container>
inline DaysOfWeek daysOfWeek(const Container<Qt::DayOfWeek>& days)
{
    return std::accumulate(days.cbegin(), days.cend(), DaysOfWeek(),
        [](DaysOfWeek a, Qt::DayOfWeek b) -> DaysOfWeek
        {
            return a | dayOfWeek(b);
        });
}

} // namespace api
} // namespace vms
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::DayOfWeek, (metatype)(numeric)(debug), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::DaysOfWeek, (metatype)(numeric)(debug), NX_VMS_API)
