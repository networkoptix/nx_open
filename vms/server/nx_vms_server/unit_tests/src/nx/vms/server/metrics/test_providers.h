#include <nx/vms/server/metrics/providers.h>

namespace nx::vms::server::metrics::test {
namespace {

QnUuid uuid(int i) { return QnUuid("00000000-0000-0000-0000-00000000000" + QString::number(i)); }

class NameProvider: public AbstractParameterProvider
{
public:
    virtual api::metrics::ParameterGroupManifest manifest() override
    {
        return api::metrics::makeParameterManifest("name", "name parameter");
    }

    virtual std::unique_ptr<Monitor> monitor(QnResourcePtr resource) override
    {
        return std::make_unique<ValueMonitor>(resource->getName());
    }

private:
    struct ValueMonitor: public Monitor
    {
        QString name;
        ValueMonitor(QString name): name(std::move(name)) {}
        virtual void start(DataBase::Access access) override { access->update(name); }
    };
};

class NumberProvider: public AbstractParameterProvider
{
public:
    virtual api::metrics::ParameterGroupManifest manifest() override
    {
        return api::metrics::makeParameterManifest("number", "number parameter", "%");
    }

    virtual std::unique_ptr<Monitor> monitor(QnResourcePtr resource) override
    {
        return std::make_unique<ValueMonitor>(resource->getName().split(" ")[1].toInt());
    }

private:
    struct ValueMonitor: public Monitor
    {
        int number = 0;
        ValueMonitor(int number): number(number) {}
        virtual void start(DataBase::Access access) override { access->update(number); };
    };
};

class TestResourceProvider: public AbstractResourceProvider
{
public:
    template<typename... Numbers>
    explicit TestResourceProvider(Numbers... numbers): m_numbers{numbers...} {}

    virtual std::unique_ptr<ParameterGroupProvider> parameters() override
    {
        return std::make_unique<ParameterGroupProvider>(
            "tests", "test resources", makeParameterProviders(
                std::make_unique<NameProvider>(),
                std::make_unique<NumberProvider>(),
                std::make_unique<ParameterGroupProvider>(
                    "group", "group in resource", makeParameterProviders(
                        std::make_unique<NameProvider>(),
                        std::make_unique<NumberProvider>()
                    )
                )
            )
        );
    }

    virtual void startDiscovery(ResourceHandler found, ResourceHandler /*lost*/) override
    {
        const auto systemId = QnUuid::createUuid();
        for (int number: m_numbers)
        {
            QnResourcePtr resource(new QnResource());
            resource->setId(uuid(number));
            resource->setName("Test " + QString::number(number));
            resource->setParentId(systemId);
            found(resource);
        }
    }

private:
    std::vector<int> m_numbers;
};

} // namespace
} // namespace nx::vms::server::metrics::test
