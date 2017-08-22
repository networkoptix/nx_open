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

SensorDescription CombinedSensorsDescription::mainSensor() const
{
    if (isEmpty())
        return SensorDescription();

    const auto it = std::find_if(begin(), end(),
        [](const SensorDescription& sensor)
        {
            return sensor.type == SensorDescription::Type::regular;
        });

    return it != end() ? *it : first();
}

bool SensorDescription::isValid() const
{
    return geometry.isValid();
}

} // namespace resource
} // namespace core
} // namespace common
} // namespace vms
} // namespace nx
