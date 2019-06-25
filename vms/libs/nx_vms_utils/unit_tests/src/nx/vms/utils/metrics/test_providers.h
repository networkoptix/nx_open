#include <nx/vms/utils/metrics/resource_providers.h>

namespace nx::vms::utils::metrics::test {
namespace {

using TestResource
    = std::shared_ptr<ResourceDescription>;

class TestResourceProvider: public ResourceProvider<TestResource>
{
public:
    template<typename... Numbers>
    explicit TestResourceProvider(Numbers... numbers)
    :
        ResourceProvider<TestResource>(makeProviders()),
        m_numbers{numbers...}
    {
    }

private:
    void startMonitoring() override
    {
        for (int number: m_numbers)
        {
            const auto suffix = QString::number(number);
            ResourceDescription resource{"test_" + suffix, "system_x", "Test " + suffix};
            found(std::make_shared<ResourceDescription>(std::move(resource)));
        }
    }

    std::optional<ResourceDescription> describe(const TestResource& resource) const override
    {
        return *resource;
    }

    static ResourceParameterProviders<TestResource> makeProviders()
    {
        return parameterProviders(
            singleParameterProvider(
                {"name", "name parameter"},
                [](const auto& resource) { return Value(resource->name); }
            ),
            singleParameterProvider(
                {"number", "number parameter"},
                [](const auto& resource) { return Value(resource->name.split(" ")[1].toInt()); }
            ),
            parameterGroupProvider(
                {"group", "group in resource"},
                singleParameterProvider(
                    {"name", "name parameter"},
                    [](const auto& resource) { return Value(resource->name); }
                ),
                singleParameterProvider(
                    {"number", "number parameter"},
                    [](const auto& resource) { return Value(resource->name.split(" ")[1].toInt()); }
                )
            )
        );
    }

private:
    std::vector<int> m_numbers;
};

} // namespace
} // namespace nx::vms::utils::metrics::test
