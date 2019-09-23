#include "camera_controller.h"

#include <nx/fusion/serialization/lexical.h>
#include <core/resource/resource_fwd.h>
#include <core/resource_management/resource_pool.h>

#include "helpers.h"

namespace nx::vms::server::metrics {

using namespace nx::vms::api;
using namespace nx::vms::server::resource;

CameraController::CameraController(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule),
    utils::metrics::ResourceControllerImpl<resource::Camera*>("cameras", makeProviders())
{
}

void CameraController::start()
{
    const auto currentServerId = serverModule()->commonModule()->moduleGUID();
    const auto resourcePool = serverModule()->commonModule()->resourcePool();
    QObject::connect(
        resourcePool, &QnResourcePool::resourceAdded,
        [this](const QnResourcePtr& resource)
        {
            if (auto camera = resource.dynamicCast<resource::Camera>().get())
            {
                const auto addOrUpdate =
                    [this, camera]()
                    {
                        add(camera, camera->getId(), (camera->getParentId() == moduleGUID())
                            ? utils::metrics::Scope::local
                            : utils::metrics::Scope::system);
                    };

                connect(camera, &QnResource::parentIdChanged, addOrUpdate);
                addOrUpdate();
            }
        });

    QObject::connect(
        resourcePool, &QnResourcePool::resourceRemoved,
        [this](const QnResourcePtr& resource)
        {
            if (auto camera = resource.dynamicCast<resource::Camera>().get())
                remove(camera->getId());
        });
}

utils::metrics::ValueGroupProviders<CameraController::Resource> CameraController::makeProviders()
{
    static const std::chrono::minutes kIssuesRateUpdateInterval(1);
    static const std::chrono::seconds kipConflictsRateUpdateInterval(15);

    return nx::utils::make_container<utils::metrics::ValueGroupProviders<Resource>>(
        utils::metrics::makeValueGroupProvider<Resource>(
            "info",
            utils::metrics::makeSystemValueProvider<Resource>(
                "name", [](const auto& r) { return Value(r->getName()); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "server", [](const auto& r) { return Value(r->getParentId().toSimpleString()); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "type", [](const auto&) { return Value(); } // TODO: Get actual type.
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "ip", [](const auto& r) { return Value(r->getHostAddress()); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "vendor", [](const auto& r) { return Value(r->getVendor()); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "model", [](const auto& r) { return Value(r->getModel()); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "firmware", [](const auto& r) { return Value(r->getFirmware()); }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "recording",
                [](const auto& r) { return Value(QnLexical::serialized(r->recordingState())); }
            )
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "availability",
            utils::metrics::makeSystemValueProvider<Resource>(
                "status",
                [](const auto& r) { return Value(QnLexical::serialized(r->getStatus())); },
                qtSignalWatch<Resource>(&resource::Camera::statusChanged)
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "streamIssues", [](const auto& r)
                { return Value(r->getAndResetMetric(&Camera::Metrics::streamIssues)); },
                nx::vms::server::metrics::timerWatch<Camera*>(kIssuesRateUpdateInterval)
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "ipConflicts", [](const auto& r)
                { return Value(r->getAndResetMetric(&Camera::Metrics::ipConflicts)); },
                nx::vms::server::metrics::timerWatch<Camera*>(kipConflictsRateUpdateInterval)
            )
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "primaryStream",
            utils::metrics::makeSystemValueProvider<Resource>(
                "resolution", [](const auto& r)
                {
                    const auto resolution = r->getLiveParams(StreamIndex::primary).resolution;
                    return Value(CameraMediaStreamInfo::resolutionToString(resolution));
                }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "fps", [](const auto& r)
                { return Value(round(r->getLiveParams(StreamIndex::primary).fps));}
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "actualFps", [](const auto& r)
                { return Value(round(r->getActualParams(StreamIndex::primary).fps));}
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "actualBitrate", [](const auto& r) //< KBps.
                { return Value(r->getActualParams(StreamIndex::primary).bitrateKbps);}
            )
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "secondaryStream",
            utils::metrics::makeSystemValueProvider<Resource>(
                "resolution", [](const auto& r)
                {
                    const auto resolution = r->getLiveParams(StreamIndex::secondary).resolution;
                    return Value(CameraMediaStreamInfo::resolutionToString(resolution));
                }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "fps", [](const auto& r)
                { return Value(round(r->getLiveParams(StreamIndex::secondary).fps));}
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "actualFps", [](const auto& r)
                { return Value(round(r->getActualParams(StreamIndex::secondary).fps));}
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "actualBitrate", [](const auto& r) //< KBps.
                { return Value(r->getActualParams(StreamIndex::secondary).bitrateKbps);}
            )
        )
        // TODO: Implement other groups.
    );
}

} // namespace nx::vms::server::metrics
