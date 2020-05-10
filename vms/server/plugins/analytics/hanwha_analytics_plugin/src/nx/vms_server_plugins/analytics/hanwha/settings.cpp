#include "settings.h"

#include <algorithm>

namespace nx::vms_server_plugins::analytics::hanwha {

bool Settings::IntelligentVideoIsActive() const
{
    bool isActive = std::any_of(std::cbegin(ivaLines), std::cend(ivaLines),
        [](const IvaLine& ivaLine)
        {
            return ivaLine.IsActive();
        });

    isActive = isActive || std::any_of(std::cbegin(ivaAreas), std::cend(ivaAreas),
        [](const IvaArea& ivaArea)
        {
            return ivaArea.IsActive();
        });

    return isActive;
}

} // namespace nx::vms_server_plugins::analytics::hanwha
