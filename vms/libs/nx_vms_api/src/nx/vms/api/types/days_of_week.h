#pragma once

#include <algorithm>

#include <QtCore/QList>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace vms {
namespace api {

enum class DayOfWeek
{
    /**%apidoc No days
     * %caption 0
     */
    none = 0,

    /**%apidoc Monday
     * %caption 2
     */
    monday    = 1 << Qt::Monday,
    
    /**%apidoc Tuesday
     * %caption 4
     */
    tuesday   = 1 << Qt::Tuesday,
    
    /**%apidoc Wednesday
     * %caption 8
     */
    wednesday = 1 << Qt::Wednesday,
    
    /**%apidoc Thursday
     * %caption 16
     */
    thursday = 1 << Qt::Thursday,
    
    /**%apidoc Friday
     * %caption 32
     */
    friday = 1 << Qt::Friday,
    
    /**%apidoc Saturday
     * %caption 64
     */
    saturday = 1 << Qt::Saturday,
    
    /**%apidoc Sunday
     * %caption 128
     */
    sunday = 1 << Qt::Sunday,
   
    /**%apidoc Workdays (all except Saturday and Sunday)
     * %caption 62
     */
    weekdays = monday | tuesday | wednesday | thursday | friday,
    
    /**%apidoc Weekend (Saturday and Sunday)
     * %caption 192
     */
    weekends = saturday | sunday,
    
    /**%apidoc All days
     * %caption 254
     */
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
