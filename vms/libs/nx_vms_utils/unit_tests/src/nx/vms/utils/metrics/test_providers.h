#pragma once

#include <nx/vms/utils/metrics/resource_controller_impl.h>

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

    void update(const QString& name, api::metrics::Value value)
    {
        NX_VERBOSE(this, "Set %1 = %2", name, value);
        auto& param = m_params[name];
        param.value = std::move(value);
        if (param.change)
            param.change();
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
    const bool m_isLocal = false;
    mutable std::map<QString, Param> m_params;
};

struct TestResourceDescription: ResourceDescription<TestResource>
{
    using ResourceDescription::ResourceDescription;

    QString id() const override { return "R" + QString::number(resource.id()); }
    QString name() const override { return "Resource " + QString::number(resource.id()); }
    QString parent() const override { return "SYSTEM_X"; }
};

class TestResourceController: public ResourceControllerImpl<TestResource>
{
public:
    TestResourceController():
        ResourceControllerImpl<TestResource>(QString("tests"), makeProviders())
    {
    }

    TestResource* makeResource(int id, bool isLocal)
    {
        std::unique_ptr<TestResourceDescription> description = std::make_unique<TestResourceDescription>(id, isLocal);
        auto resource = &description->resource;

        resource->update("i1", id * 10 + 1);
        resource->update("t1", "first of " + QString::number(id));

        resource->update("i2", id * 10 + 2);
        resource->update("t2", "second of " + QString::number(id));

        add(std::move(description));
        return resource;
    }

private:
    void start() override { /* Resources are normally created by makeResource(id). */ }

    static ValueGroupProviders<Resource> makeProviders()
    {
        return nx::utils::make_container<ValueGroupProviders<Resource>>(
            std::make_unique<ValueGroupProvider<Resource>>(
                api::metrics::Label{
                    "g1", "group 1"
                },
                std::make_unique<ValueProvider<Resource>>(
                    api::metrics::ValueManifest{
                        "i", "int parameter", "table&panel", ""
                    },
                    Getter<Resource>([](const auto& r) { return r.current("i1"); }),
                    Watch<Resource>([](const auto& r, auto change) { return r.monitor("i1", std::move(change)); })
                ),
                std::make_unique<ValueProvider<Resource>>(
                    api::metrics::ValueManifest{
                        "t", "text parameter", "panel", ""
                    },
                    [](const auto& r) { return r.current("t1"); },
                    [](const auto& r, auto change) { return r.monitor("t1", std::move(change)); }
                )
            ),
            std::make_unique<ValueGroupProvider<Resource>>(
                api::metrics::Label{
                    "g2", "group 2"
                },
                std::make_unique<ValueProvider<Resource>>(
                    api::metrics::ValueManifest{
                        "i", "int parameter", "panel", ""
                    },
                    Getter<Resource>([](const auto& r) { return r.current("i2"); }),
                    Watch<Resource>([](const auto& r, auto change) { return r.monitor("i2", std::move(change)); })
                ),
                std::make_unique<ValueProvider<Resource>>(
                    api::metrics::ValueManifest{
                        "t", "text parameter", "table&panel", ""
                    },
                    [](const auto& r) { return r.current("t2"); },
                    [](const auto& r, auto change) { return r.monitor("t2", std::move(change)); }
                )
            )
        );
    }
};

} // namespace
} // namespace nx::vms::utils::metrics::test
