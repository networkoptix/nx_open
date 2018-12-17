#pragma once

#include <nx/utils/std/optional.h>

namespace nx::analytics {

template<typename Descriptor>
class DefaultDescriptorMerger
{
public:
    std::optional<Descriptor> operator()(
        const Descriptor* firstDescriptor,
        const Descriptor* secondDescriptor) const
    {
        if (secondDescriptor)
            return *secondDescriptor;

        if (firstDescriptor)
            return *firstDescriptor;

        return std::nullopt;
    }
};

} // namespace nx::analytics
