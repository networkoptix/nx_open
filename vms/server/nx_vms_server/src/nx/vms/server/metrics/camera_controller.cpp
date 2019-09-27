#include "camera_controller.h"

#include <nx/fusion/serialization/lexical.h>
#include <core/resource/resource_fwd.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/text/archive_duration.h>

#include "helpers.h"

namespace nx::vms::server::metrics {

using namespace nx::vms::api;
using namespace nx::vms::server::resource;
using namespace std::chrono;

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
    static std::chrono::hours kBitratePeriod(24);
    static const std::chrono::seconds kFpsDeltaCheckInterval(5);

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
                "type", [](const auto& r) { return Value(QnLexical::serialized(r->deviceType())); }
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
            utils::metrics::makeLocalValueProvider<Resource>(
                "streamIssues",
                [](const auto& r)
                {
                    return Value(r->getAndResetMetric(&Camera::Metrics::streamIssues));
                },
                nx::vms::server::metrics::timerWatch<Camera*>(kIssuesRateUpdateInterval)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "ipConflicts",
                [](const auto& r)
                {
                    return Value(r->getAndResetMetric(&Camera::Metrics::ipConflicts));
                },
                nx::vms::server::metrics::timerWatch<Camera*>(kipConflictsRateUpdateInterval)
            )
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "primaryStream",
            utils::metrics::makeSystemValueProvider<Resource>(
                "resolution",
                [](const auto& r)
                {
                    const auto resolution = r->getLiveParams(StreamIndex::primary).resolution;
                    return Value(CameraMediaStreamInfo::resolutionToString(resolution));
                }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "targetFps",
                [](const auto& r)
                {
                    return Value(r->getLiveParams(StreamIndex::primary).fps);
                }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "actualFps",
                [](const auto& r)
                {
                    return Value(r->getActualParams(StreamIndex::primary).fps);
                }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "fpsDelta",
                [](const auto& r)
                {
                    const auto fpsDelta = r->getLiveParams(StreamIndex::primary).fps -
                        r->getActualParams(StreamIndex::primary).fps;
                    return Value(fpsDelta);
                },
                nx::vms::server::metrics::timerWatch<Camera*>(kFpsDeltaCheckInterval)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "actualBitrateKBps",
                [](const auto& r)
                {
                    return Value(r->getActualParams(StreamIndex::primary).bitrateKbps);
                }
            )
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "secondaryStream",
            utils::metrics::makeSystemValueProvider<Resource>(
                "resolution",
                [](const auto& r)
                {
                    const auto resolution = r->getLiveParams(StreamIndex::secondary).resolution;
                    return Value(CameraMediaStreamInfo::resolutionToString(resolution));
                }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "targetFps",
                [](const auto& r)
                { return Value(r->getLiveParams(StreamIndex::secondary).fps);}
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "actualFps",
                [](const auto& r)
                {
                    return Value(r->getActualParams(StreamIndex::secondary).fps);
                }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "actualBitrateKBps",
                [](const auto& r)
                { return Value(r->getActualParams(StreamIndex::secondary).bitrateKbps);}
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "fpsDelta",
                [](const auto& r)
                {
                    const auto fpsDelta = r->getLiveParams(StreamIndex::secondary).fps -
                        r->getActualParams(StreamIndex::secondary).fps;
                    return Value(fpsDelta);
                },
                nx::vms::server::metrics::timerWatch<Camera*>(kFpsDeltaCheckInterval)
            )
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "analytics",
            utils::metrics::makeLocalValueProvider<Resource>(
                "archiveLength",
                [](const auto& r)
                {
                    const auto value = duration_cast<seconds>(r->nxOccupiedDuration());
                    return Value(nx::vms::text::ArchiveDuration::durationToString(value));
                }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "recordingBitrateKBps",
                [](const auto& r) { return Value(r->recordingBitrateKBps(kBitratePeriod)); }
            )
        )
    );
}

} // namespace nx::vms::server::metrics
