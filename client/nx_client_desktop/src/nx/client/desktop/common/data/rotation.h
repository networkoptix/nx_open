#pragma once

#include <QtCore/QMetaType>

namespace nx {
namespace client {
namespace desktop {

class Rotation
{
public:
    Rotation() = default;
    explicit Rotation(qreal degrees);

    qreal value() const;
    QString toString() const;

    static QList<Rotation> standardRotations();
    static Rotation closestStandardRotation(qreal degrees);

    bool operator==(const Rotation& other) const;
    bool operator!=(const Rotation& other) const;

private:
    qreal m_degrees = 0;
};

} // namespace desktop
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::desktop::Rotation)
