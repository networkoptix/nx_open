#pragma once

#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/utils/metrics/resource_controller_impl.h>
#include <nx/utils/safe_direct_connection.h>
#include <core/resource/media_server_resource.h>

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

    qint64 getMetric(Metrics parameter);
    qint64 getDelta(Metrics key, qint64 value);
    void at_syncTimeChanged(qint64 syncTime);
private:
    std::vector<std::atomic<qint64>> m_counters;
    int m_timeChangeEvents = 0;
};

} // namespace nx::vms::server::metrics
