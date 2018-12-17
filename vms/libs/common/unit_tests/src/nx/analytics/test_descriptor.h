#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx::analytics {

struct TestDescriptor
{
    TestDescriptor() = default;
    TestDescriptor(std::string id, std::string name):
        id(std::move(id)),
        name(std::move(name))
    {
    }

    TestDescriptor(std::string id, std::string name, int mergeCounter):
        id(std::move(id)),
        name(std::move(name)),
        mergeCounter(mergeCounter)
    {
    }

    std::string getId() { return id; };
    bool operator==(const TestDescriptor& other) const
    {
        return id == other.id && name == other.name && mergeCounter == other.mergeCounter;
    }

    std::string id;
    std::string name;
    int mergeCounter = 0;
};

#define nx_analytics_TestDescriptor_Fields (id)(name)(mergeCounter)

QN_FUSION_DECLARE_FUNCTIONS(TestDescriptor, (json))

} // namespace nx::analytics

