#pragma once

#include <QtCore/QMetaType>

#include <nx/utils/std/optional.h>

namespace nx {
namespace client {
namespace desktop {

class Rotation
{
public:
    Rotation() = default;
    explicit Rotation(qreal degrees);

    bool isValid() const;

    qreal value() const;
    QString toString() const;

    static QList<Rotation> standardRotations();
    static Rotation closestStandardRotation(qreal degrees);

    bool operator==(const Rotation& other) const;
    bool operator!=(const Rotation& other) const;

private:
    std::optional<qreal> m_degrees;
};

} // namespace desktop
} // namespace client
} // namespace nx

Q_DECLARE_METATYPE(nx::client::desktop::Rotation)
