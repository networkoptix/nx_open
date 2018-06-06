#pragma once

#include <array>

#include <QtGui/QVector2D>
#include <QtCore/QFlags>

#include <utils/math/math.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace core {
namespace ptz {

enum class PtzComponent
{
    pan = 1 << 0,
    tilt = 1 << 1,
    rotation = 1 << 2,
    zoom = 1 << 3
};
Q_DECLARE_FLAGS(PtzComponents, PtzComponent)

template<int VectorSize>
using ComponentVector = std::array<PtzComponent, VectorSize>;

struct PtzVector
{

public:
    static const ComponentVector<2> kPtComponents;
    static const ComponentVector<3> kPtrComponents;
    static const ComponentVector<3> kPtzComponents;

public:
    PtzVector() = default;
    PtzVector(double pan, double tilt, double rotation, double zoom);

    PtzVector(
        const QPointF& point,
        const ComponentVector<2>& components = { PtzComponent::pan, PtzComponent::tilt });

    PtzVector(
        const QVector3D& vector,
        const ComponentVector<3>& components);

    void setComponent(double value, PtzComponent component);

    double pan = 0.0;
    double tilt = 0.0;
    double rotation = 0.0;
    double zoom = 0.0;

    // If you need more freedom degrees fill free to extend this struct.

    PtzVector operator+(const PtzVector& other) const;

    PtzVector operator-(const PtzVector& other) const;

    // Multiplies the corresponding components of two vectors.
    // Is not the same as cross profuct.
    PtzVector operator*(const PtzVector& other) const;

    PtzVector operator/(const PtzVector& other) const;

    PtzVector operator/(double scalar) const;

    double length() const;

    double lengthSquared() const;

    QVector2D toQVector2D() const;

    bool isNull() const;
};

PtzVector operator*(const PtzVector& ptzVector, double scalar);
PtzVector operator*(double scalar, const PtzVector& ptzVector);

Q_DECLARE_OPERATORS_FOR_FLAGS(PtzComponents)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(PtzComponent)

#define PtzVector_Fields (pan)(tilt)(rotation)(zoom)
QN_FUSION_DECLARE_FUNCTIONS(PtzVector, (json)(eq))

} // namespace ptz
} // namespace core
} // namespace nx

Q_DECLARE_METATYPE(nx::core::ptz::PtzVector);
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (nx::core::ptz::PtzComponent)
    (nx::core::ptz::PtzComponents),
    (metatype)(numeric)(lexical))

QDebug operator<<(QDebug dbg, const nx::core::ptz::PtzVector& ptzVector);

inline bool qFuzzyEquals(const nx::core::ptz::PtzVector& lhs, const nx::core::ptz::PtzVector& rhs)
{
    return ::qFuzzyEquals(lhs.pan, rhs.pan)
        && ::qFuzzyEquals(lhs.tilt, rhs.tilt)
        && ::qFuzzyEquals(lhs.rotation, rhs.rotation)
        && ::qFuzzyEquals(lhs.zoom, rhs.zoom);
}

inline bool qFuzzyIsNull(const nx::core::ptz::PtzVector& ptzVector)
{
    return ::qFuzzyIsNull(ptzVector.pan)
        && ::qFuzzyIsNull(ptzVector.tilt)
        && ::qFuzzyIsNull(ptzVector.rotation)
        && ::qFuzzyIsNull(ptzVector.zoom);
}

inline bool qIsNaN(const nx::core::ptz::PtzVector& ptzVector)
{
    return ::qIsNaN(ptzVector.pan)
        || ::qIsNaN(ptzVector.tilt)
        || ::qIsNaN(ptzVector.rotation)
        || ::qIsNaN(ptzVector.zoom);
}

template<>
inline nx::core::ptz::PtzVector qQNaN<nx::core::ptz::PtzVector>()
{
    static const double kNan = ::qQNaN<double>();
    return nx::core::ptz::PtzVector(kNan, kNan, kNan, kNan);
};

template<>
inline nx::core::ptz::PtzVector qSNaN<nx::core::ptz::PtzVector>()
{
    static const double kNan = ::qSNaN<double>();
    return nx::core::ptz::PtzVector(kNan, kNan, kNan, kNan);
}

