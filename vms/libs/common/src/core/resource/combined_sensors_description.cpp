#include "combined_sensors_description.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace vms {
namespace common {
namespace core {
namespace resource {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SensorDescription, (json), (type)(geometry))
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(
        nx::vms::common::core::resource::SensorDescription, Type)

SensorDescription::SensorDescription(const QRectF& geometry, SensorDescription::Type type):
    type(type),
    geometry(geometry)
{
}

bool SensorDescription::isValid() const
{
    return geometry.isValid();
}

QRect SensorDescription::frameSubRect(const QSize& frameSize) const
{
    if (!isValid())
        return QRect();

    return QRectF(
        frameSize.width() * geometry.x(),
        frameSize.height() * geometry.y(),
        frameSize.width() * geometry.width(),
        frameSize.height() * geometry.height()).toRect();
}

SensorDescription CombinedSensorsDescription::getSensor(SensorDescription::Type type) const
{
    if (isEmpty())
        return SensorDescription();

    const auto it = std::find_if(begin(), end(),
        [type](const SensorDescription& sensor)
        {
            return sensor.type == type;
        });

    return it != end() ? *it : SensorDescription();
}

SensorDescription CombinedSensorsDescription::mainSensor() const
{
    if (isEmpty())
        return SensorDescription();

    const auto& sensor = colorSensor();
    if (sensor.isValid())
        return sensor;

    return first();
}

SensorDescription CombinedSensorsDescription::colorSensor() const
{
    return getSensor(SensorDescription::Type::regular);
}

SensorDescription CombinedSensorsDescription::blackAndWhiteSensor() const
{
    return getSensor(SensorDescription::Type::blackAndWhite);
}

} // namespace resource
} // namespace core
} // namespace common
} // namespace vms
} // namespace nx
