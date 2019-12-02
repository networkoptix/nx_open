#pragma once

#include <core/resource/media_server_resource.h>
#include <nx/utils/safe_direct_connection.h>
#include <nx/utils/value_cache.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/utils/metrics/resource_controller_impl.h>

namespace nx::vms::server { class PlatformMonitor; }

namespace nx::vms::server::metrics {

/**
 * Provides a single server (the current one).
 */
class ServerController:
    public ServerModuleAware,
    public utils::metrics::ResourceControllerImpl<QnMediaServerResource*>,
    public /*mixin*/ Qn::EnableSafeDirectConnection
{
public:
    ServerController(QnMediaServerModule* serverModule);
    virtual ~ServerController() override;
    void start() override;

private:
    void beforeValues(utils::metrics::Scope requestScope, bool formatted) override;
    void beforeAlarms(utils::metrics::Scope requestScope) override;

    enum class Metrics
    {
        transactions,
        timeChanged,
        decodedPixels,
        encodedPixels,
        ruleActionsTriggered,
        thumbnailsRequested,
        apiCalls,
        count
    };

    utils::metrics::ValueGroupProviders<Resource> makeProviders();
    utils::metrics::ValueProviders<Resource> makeAvailabilityProviders();
    utils::metrics::ValueProviders<Resource> makeLoadProviders();
    utils::metrics::ValueProviders<Resource> makeInfoProviders();
    utils::metrics::ValueProviders<Resource> makeActivityProviders();

    qint64 getMetric(Metrics parameter);
    qint64 getDelta(Metrics key, qint64 value);
    nx::vms::server::PlatformMonitor* platform() const;
    QString dateTimeToString(const QDateTime& datetime) const;

private:
    std::vector<std::atomic<qint64>> m_counters;
    std::atomic<int> m_timeChangeEvents = 0;
    nx::utils::CachedValue<QDateTime> m_currentDateTime;
};

} // namespace nx::vms::server::metrics
