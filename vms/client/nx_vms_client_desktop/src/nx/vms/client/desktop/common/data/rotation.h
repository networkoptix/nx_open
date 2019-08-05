#pragma once

#include <QtCore/QMetaType>

namespace nx::vms::client::desktop {

class Rotation
{
public:
    Rotation() = default;
    explicit Rotation(qreal degrees);

    qreal value() const;
    QString toString() const;

    using RotationList = QList<Rotation>;
    static RotationList standardRotations();
    static Rotation closestStandardRotation(qreal degrees);

    bool operator==(const Rotation& other) const;
    bool operator!=(const Rotation& other) const;

private:
    qreal m_degrees = 0;
};

} // namespace nx::vms::client::desktop

Q_DECLARE_METATYPE(nx::vms::client::desktop::Rotation)
