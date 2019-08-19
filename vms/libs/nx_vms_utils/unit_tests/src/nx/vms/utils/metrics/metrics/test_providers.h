#include <nx/vms/utils/metrics/resource_providers.h>

namespace nx::vms::utils::metrics::test {
namespace {

class TestResource
{
public:
    TestResource(int id, bool isLocal):
        m_id(id),
        m_isLocal(isLocal)
    {
    }

    int id() const { return m_id; }
    bool isLocal() const { return m_isLocal; }

    void update(const QString& name, Value value)
    {
        auto& param = m_params[name];
        param.value = std::move(value);
        if (param.change)
            param.change();
    }

    Value current(const QString& name) const
    {
        return m_params.find(name)->second.value;
    }

    nx::utils::SharedGuardPtr monitor(const QString& name, nx::utils::MoveOnlyFunc<void()> change)
    {
        m_params[name].change = std::move(change);
        return nx::utils::makeSharedGuard([](){});
    }

private:
    struct Param
    {
        Value value;
        nx::utils::MoveOnlyFunc<void()> change;
    };

private:
    const int m_id = 0;
    const bool m_isLocal = false;
    std::map<QString, Param> m_params;
};

class TestResourceProvider: public ResourceProvider<std::shared_ptr<TestResource>>
{
public:
    TestResourceProvider(): ResourceProvider<std::shared_ptr<TestResource>>(makeProviders()) {}

    std::shared_ptr<TestResource> makeResource(int id, bool isLocal)
    {
        auto resource = std::make_shared<TestResource>(id, isLocal);

        resource->update("i", id);
        resource->update("t", "text_a" + QString::number(id));

        resource->update("gi", 10 + id);
        resource->update("gt", "text_b" + QString::number(id));

        found(resource);
        return resource;
    }

private:
    void startMonitoring() override
    {
        // Resources are normally created by makeResource(id).
    }

    std::optional<ResourceDescription> describe(
        const std::shared_ptr<TestResource>& resource) const override
    {
        const auto id = QString::number(resource->id());
        return ResourceDescription("test_" + id, "system_x", "Test " + id, resource->isLocal());
    }

    static ResourceParameterProviders<std::shared_ptr<TestResource>> makeProviders()
    {
        return parameterProviders(
            singleParameterProvider(
                {"i", "int parameter"},
                [](const auto& r) { return r->current("i"); },
                [](const auto& r, auto change) { return r->monitor("i", std::move(change)); }
            ),
            singleParameterProvider(
                {"t", "text parameter"},
                [](const auto& r) { return r->current("t"); },
                [](const auto& r, auto change) { return r->monitor("t", std::move(change)); }
            ),
            parameterGroupProvider(
                {"g", "group in resource"},
                singleParameterProvider(
                    {"i", "int parameter"},
                    [](const auto& r) { return r->current("gi"); },
                    [](const auto& r, auto change) { return r->monitor("gi", std::move(change)); }
                ),
                singleParameterProvider(
                    {"t", "text parameter"},
                    [](const auto& r) { return r->current("gt"); },
                    [](const auto& r, auto change) { return r->monitor("gt", std::move(change)); }
                )
            )
        );
    }
};

} // namespace
} // namespace nx::vms::utils::metrics::test
