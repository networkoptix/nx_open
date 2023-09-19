// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <type_traits>

#include <QtCore/QtGlobal>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API Timestamp
{
    /**%apidoc:string
     * %// This is some sequence introduced to give a higher priority to transactions generated
     * %// from unbinding / binding to the Cloud.
     */
    quint64 sequence = 0;

    /**%apidoc:string
     * %// This is a regular transaction timestamp, in milliseconds since the epoch.
     */
    quint64 ticks = 0;

    Timestamp() = default;
    Timestamp(quint64 sequence, quint64 ticks): sequence(sequence), ticks(ticks) {}

    bool operator==(const Timestamp& right) const;
    bool operator!=(const Timestamp& right) const { return !(*this == right); }

    bool operator<(const Timestamp& right) const;
    bool operator<=(const Timestamp& right) const;
    bool operator>(const Timestamp& right) const { return right < (*this); }
    bool operator>=(const Timestamp& right) const { return right <= (*this); }

    bool operator>(quint64 right) const;

    Timestamp& operator-=(qint64 right);
    Timestamp& operator+=(qint64 right);

    Timestamp& operator++(); // ++t
    Timestamp operator++(int); // t++

    Timestamp operator+(quint64 right) const;

    QString toString() const;

    template<typename T>
    static Timestamp fromInteger(T value) { return Timestamp(value); }

private:
    template<typename T>
    Timestamp(T value): ticks(quint64(value))
    {
        static_assert(std::is_integral<T>::value, "T must be an integer type");
    }

    template<typename T>
    Timestamp& operator=(T value)
    {
        static_assert(std::is_integral<T>::value, "T must be an integer type");
        sequence = 0;
        ticks = quint64(value);
        return *this;
    }
};
#define Timestamp_Fields (sequence)(ticks)
NX_REFLECTION_INSTRUMENT(Timestamp, Timestamp_Fields);

QN_FUSION_DECLARE_FUNCTIONS(
    Timestamp,
    (json)(ubjson)(xml)(csv_record),
    NX_VMS_API)

} // namespace api
} // namespace vms
} // namespace nx
