#pragma once

#include <type_traits>

#include <QtCore/QtGlobal>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace vms {
namespace api {

class NX_VMS_API Timestamp
{
public:
    /**
     * This is some sequence introduced to give transactions generated after
     * unbinding from cloud / binding to cloud higher priority.
     */
    quint64 sequence = 0;

    /** This is a regular transaction timestamp. Close to millis since epoch. */
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

QN_FUSION_DECLARE_FUNCTIONS(
    Timestamp,
    (json)(ubjson)(xml)(csv_record),
    NX_VMS_API)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::Timestamp)
