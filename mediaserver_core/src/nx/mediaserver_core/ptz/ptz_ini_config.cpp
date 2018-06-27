#include "ptz_ini_config.h"

#include <nx/fusion/model_functions.h>

namespace nx {
namespace mediaserver_core {
namespace ptz {

namespace {

Ptz::Capabilities deserializeCapabilities(const QString& serialized)
{
    return QnLexical::deserialized<Ptz::Capabilities>(serialized);
}

} // namespace

Ptz::Capabilities PtzIniConfig::addedCapabilities() const
{
    return deserializeCapabilities(capabilitiesToAdd);
}

Ptz::Capabilities PtzIniConfig::excludedCapabilities() const
{
    return deserializeCapabilities(capabilitiesToRemove);
}

} // namespace ptz
} // namespace mediaserver_core
} // namespace nx
