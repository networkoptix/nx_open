#pragma once

#include <nx/utils/std/optional.h>
#include <nx/analytics/test_descriptor.h>

namespace nx::analytics {

class TestMergeExecutor
{
public:
    std::optional<TestDescriptor> operator()(
        const TestDescriptor* first,
        const TestDescriptor* second) const
    {
        if (second)
        {
            if (first)
            {
                TestDescriptor result = *second;
                result.mergeCounter = first->mergeCounter + 1;
                return result;
            }

            return *second;
        }

        if (first)
            return *first;

        return std::nullopt;
    }
};

} // namespace nx::analytics
