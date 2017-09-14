#pragma once

#include <QtCore/QRectF>
#include <QtCore/QList>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace vms {
namespace common {
namespace core {
namespace resource {

struct SensorDescription
{
    Q_GADGET

public:
    enum class Type
    {
        regular,
        blackAndWhite
    };
    Q_ENUM(Type)

    Type type;
    QRectF geometry;

    SensorDescription(const QRectF& geometry = QRectF(), Type type = Type::regular);

    bool isValid() const;

    QRect frameSubRect(const QSize& frameSize) const;
};

class CombinedSensorsDescription: public QList<SensorDescription>
{
public:
    using QList<SensorDescription>::QList;

    SensorDescription getSensor(SensorDescription::Type type) const;

    SensorDescription mainSensor() const;
    SensorDescription colorSensor() const;
    SensorDescription blackAndWhiteSensor() const;
};

QN_FUSION_DECLARE_FUNCTIONS(SensorDescription, (json))
QN_FUSION_DECLARE_FUNCTIONS(SensorDescription::Type, (lexical))

} // namespace resource
} // namespace core
} // namespace common
} // namespace vms
} // namespace nx
