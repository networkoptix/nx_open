#pragma once

#include <nx/vms/utils/metrics/resource_controller_impl.h>

namespace nx::vms::utils::metrics::test {
namespace {

class TestResource
{
public:
    TestResource(int id):
        m_id(id)
    {
        setDefaults();
    }

    int id() const { return m_id; }

    void update(const QString& name, api::metrics::Value value)
    {
        NX_VERBOSE(this, "Set %1 = %2", name, value);
        auto& param = m_params[name];
        param.value = std::move(value);
        if (param.change)
            param.change();
    }

    void setDefaults()
    {
        update("i1", m_id * 10 + 1);
        update("t1", "first of " + QString::number(m_id));
        update("i2", m_id * 10 + 2);
        update("t2", "second of " + QString::number(m_id));
    }

    api::metrics::Value current(const QString& name) const
    {
        const auto value = m_params.find(name)->second.value;
        NX_VERBOSE(this, "Return %1 = %2", name, value);
        return value;
    }

    nx::utils::SharedGuardPtr monitor(const QString& name, nx::utils::MoveOnlyFunc<void()> change) const
    {
        m_params[name].change = std::move(change);
        return nx::utils::makeSharedGuard([](){});
    }

    QString idForToStringFromPtr() const
    {
        return QString::number(m_id);
    }

private:
    struct Param
    {
        api::metrics::Value value;
        nx::utils::MoveOnlyFunc<void()> change;
    };

private:
    const int m_id = 0;
    mutable std::map<QString, Param> m_params;
};

struct TestResourceDescription: ResourceDescription<TestResource>
{
    using ResourceDescription::ResourceDescription;
    QString id() const override { return "R" + QString::number(resource.id()); }
    Scope scope() const override { return (resource.id() % 2 == 0) ? Scope::local : Scope::system; }
};

class TestResourceController: public ResourceControllerImpl<TestResource>
{
public:
    TestResourceController():
        ResourceControllerImpl<TestResource>(QString("tests"), makeProviders())
    {
    }

    TestResource* makeResource(int id)
    {
        std::unique_ptr<TestResourceDescription> description = std::make_unique<TestResourceDescription>(id);
        const auto resource = &description->resource;
        add(std::move(description));
        return resource;
    }

private:
    void start() override { /* Resources are normally created by makeResource(id). */ }

    static ValueGroupProviders<Resource> makeProviders()
    {
        return nx::utils::make_container<ValueGroupProviders<Resource>>(
            makeValueGroupProvider<Resource>(
                "g1",
                makeLocalValueProvider<Resource>(
                    "i",
                    [](const auto& r) { return r.current("i1"); },
                    [](const auto& r, auto change) { return r.monitor("i1", std::move(change)); }
                ),
                makeLocalValueProvider<Resource>(
                    "t",
                    [](const auto& r) { return r.current("t1"); }
                )
            ),
            makeValueGroupProvider<Resource>(
                "g2",
                makeSystemValueProvider<Resource>(
                    "i",
                    [](const auto& r) { return r.current("i2"); },
                    [](const auto& r, auto change) { return r.monitor("i2", std::move(change)); }
                ),
                makeSystemValueProvider<Resource>(
                    "t",
                    [](const auto& r) { return r.current("t2"); }
                )
            )
        );
    }
};

} // namespace
} // namespace nx::vms::utils::metrics::test
