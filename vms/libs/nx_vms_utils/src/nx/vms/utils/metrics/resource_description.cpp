#include "resource_description.h"

#include <nx/utils/log/assert.h>

namespace nx::vms::utils::metrics {

QString toString(Scope scope)
{
    switch (scope)
    {
        case Scope::local: return "local";
        case Scope::system: return "system";
    }

    NX_ASSERT(false, "Unexpected scope: %1", static_cast<int>(scope));
    return "";
}

ResourceDescription::ResourceDescription(QString id, Scope scope):
    id(std::move(id)),
    scope(scope)
{
}

} // namespace nx::vms::utils::metrics
