#pragma once

#include <core/resource/resource.h>
#include <nx/utils/value_history.h>
#include <nx/vms/api/metrics.h>

namespace nx::vms::server::metrics {

using DataBase = nx::utils::TreeValueHistory<QString, api::metrics::Value>;

class AbstractParameterProvider
{
public:
    virtual ~AbstractParameterProvider() = default;

    class Monitor
    {
    public:
        virtual void start(DataBase::Access access) = 0;
        virtual ~Monitor() = default;
    };

    virtual api::metrics::ParameterGroupManifest manifest() = 0;
    virtual std::unique_ptr<Monitor> monitor(QnResourcePtr resource) = 0;
};

using ParameterProviders = std::vector<std::unique_ptr<AbstractParameterProvider>>;
using ParameterMonitors = std::vector<std::unique_ptr<AbstractParameterProvider::Monitor>>;

template<typename... Providers>
ParameterProviders makeParameterProviders(Providers... providers)
{
    ParameterProviders parameters;
    (parameters.push_back(std::forward<Providers>(providers)), ...);
    return parameters;
}

class ParameterGroupProvider: public AbstractParameterProvider
{
public:
    ParameterGroupProvider(QString id, QString name, ParameterProviders providers);

    virtual api::metrics::ParameterGroupManifest manifest() override;
    virtual std::unique_ptr<Monitor> monitor(QnResourcePtr resource) override;

private:
    const QString m_id;
    const QString m_name;
    const ParameterProviders m_providers;
};

class AbstractResourceProvider
{
public:
    virtual ~AbstractResourceProvider() = default;

    using ResourceHandler = std::function<void(QnResourcePtr)>;

    virtual std::unique_ptr<ParameterGroupProvider> parameters() = 0;
    virtual void startDiscovery(ResourceHandler found, ResourceHandler lost) = 0;
};

} // namespace nx::vms::server::metrics


