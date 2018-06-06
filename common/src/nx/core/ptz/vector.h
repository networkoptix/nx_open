#pragma once

#include <QtGui/QVector2D>
#include <QtGui/QVector3D>

#include <nx/core/ptz/component.h>

#include <utils/math/math.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace core {
namespace ptz {

struct Vector
{

public:
    static const ComponentVector<2> kPtComponents;
    static const ComponentVector<3> kPtrComponents;
    static const ComponentVector<3> kPtzComponents;

public:
    Vector() = default;
    Vector(double pan, double tilt, double rotation, double zoom);

    Vector(
        const QPointF& point,
        const ComponentVector<2>& components = {Component::pan, Component::tilt});

    Vector(
        const QVector3D& vector,
        const ComponentVector<3>& components);

    void setComponent(double value, Component component);

    double pan = 0.0;
    double tilt = 0.0;
    double rotation = 0.0;
    double zoom = 0.0;

    // If you need more freedom degrees fill free to extend this struct.

    Vector operator+(const Vector& other) const;

    Vector operator-(const Vector& other) const;

    // Multiplies the corresponding components of two vectors.
    // Is not the same as cross profuct.
    Vector operator*(const Vector& other) const;

    Vector operator/(const Vector& other) const;

    Vector operator/(double scalar) const;

    double length() const;

    double lengthSquared() const;

    QVector2D toQVector2D() const;

    bool isNull() const;
};

Vector operator*(const Vector& ptzVector, double scalar);
Vector operator*(double scalar, const Vector& ptzVector);

#define PtzVector_Fields (pan)(tilt)(rotation)(zoom)
QN_FUSION_DECLARE_FUNCTIONS(Vector, (json)(eq))

} // namespace ptz
} // namespace core
} // namespace nx

Q_DECLARE_METATYPE(nx::core::ptz::Vector);

QDebug operator<<(QDebug dbg, const nx::core::ptz::Vector& ptzVector);

inline bool qFuzzyEquals(const nx::core::ptz::Vector& lhs, const nx::core::ptz::Vector& rhs)
{
    return ::qFuzzyEquals(lhs.pan, rhs.pan)
        && ::qFuzzyEquals(lhs.tilt, rhs.tilt)
        && ::qFuzzyEquals(lhs.rotation, rhs.rotation)
        && ::qFuzzyEquals(lhs.zoom, rhs.zoom);
}

inline bool qFuzzyIsNull(const nx::core::ptz::Vector& ptzVector)
{
    return ::qFuzzyIsNull(ptzVector.pan)
        && ::qFuzzyIsNull(ptzVector.tilt)
        && ::qFuzzyIsNull(ptzVector.rotation)
        && ::qFuzzyIsNull(ptzVector.zoom);
}

inline bool qIsNaN(const nx::core::ptz::Vector& ptzVector)
{
    return ::qIsNaN(ptzVector.pan)
        || ::qIsNaN(ptzVector.tilt)
        || ::qIsNaN(ptzVector.rotation)
        || ::qIsNaN(ptzVector.zoom);
}

template<>
inline nx::core::ptz::Vector qQNaN<nx::core::ptz::Vector>()
{
    static const double kNan = ::qQNaN<double>();
    return nx::core::ptz::Vector(kNan, kNan, kNan, kNan);
};

template<>
inline nx::core::ptz::Vector qSNaN<nx::core::ptz::Vector>()
{
    static const double kNan = ::qSNaN<double>();
    return nx::core::ptz::Vector(kNan, kNan, kNan, kNan);
}

