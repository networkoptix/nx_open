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
using namespace nx::vms::text;

using Resource = CameraController::Resource;
using Value = CameraController::Value;

static const std::chrono::minutes kIssuesRateUpdateInterval(1);
static const std::chrono::seconds kipConflictsRateUpdateInterval(15);
static std::chrono::hours kBitratePeriod(24);
static const std::chrono::seconds kFpsDeltaCheckInterval(5);

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
                if (camera->hasFlags(Qn::desktop_camera))
                    return; //< Ignore desktop cameras.
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

auto makeInfoGroupProvider()
{
    return
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
        );
}

auto makeAvailabilityGroupProvider()
{
    return
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
        );
}

auto makePrimaryStreamGroupProvider()
{
    return
        utils::metrics::makeValueGroupProvider<Resource>(
            "primaryStream",
            utils::metrics::makeSystemValueProvider<Resource>(
                "resolution",
                [](const auto& r)
                {
                    if (auto p = r->targetParams(StreamIndex::primary))
                        return Value(CameraMediaStreamInfo::resolutionToString(p->resolution));
                    return Value();
                }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "targetFps",
                [](const auto& r)
                {
                    if (auto params = r->targetParams(StreamIndex::primary))
                        return Value(params->fps);
                    return Value();
                }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "actualFps",
                [](const auto& r)
                {
                    if (auto params = r->actualParams(StreamIndex::primary))
                        return Value(params->fps);
                    return Value();
                }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "fpsDelta",
                [](const auto& r)
                {
                    const auto targetParams = r->targetParams(StreamIndex::primary);
                    const auto actualParams = r->actualParams(StreamIndex::primary);
                    if (targetParams && actualParams)
                        return Value(targetParams->fps - actualParams->fps);
                    return Value();
                },
                nx::vms::server::metrics::timerWatch<Camera*>(kFpsDeltaCheckInterval)
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "actualBitrateBps",
                [](const auto& r)
                {
                    if (auto params = r->actualParams(StreamIndex::primary))
                        return Value(params->bitrateKbps * 1024);
                    return Value();
                }
            )
        );
}

auto makeSecondaryStreamGroupProvider()
{
    return
        utils::metrics::makeValueGroupProvider<Resource>(
            "secondaryStream",
            utils::metrics::makeSystemValueProvider<Resource>(
                "resolution",
                [](const auto& r)
                {
                    if (auto p = r->targetParams(StreamIndex::secondary))
                        return Value(CameraMediaStreamInfo::resolutionToString(p->resolution));
                    return Value();
                }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "targetFps",
                [](const auto& r)
                {
                    if (auto params = r->targetParams(StreamIndex::secondary))
                        return Value(params->fps);
                    return Value();
                }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "actualFps",
                [](const auto& r)
                {
                    if (auto params = r->actualParams(StreamIndex::secondary))
                        return Value(params->fps);
                    return Value();
                }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "actualBitrateBps",
                [](const auto& r)
                {
                    if (auto params = r->actualParams(StreamIndex::secondary))
                        return Value(params->bitrateKbps * 1024);
                    return Value();
                }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "fpsDelta",
                [](const auto& r)
                {
                    const auto targetParams = r->targetParams(StreamIndex::secondary);
                    const auto actualParams = r->actualParams(StreamIndex::secondary);
                    if (targetParams && actualParams)
                        return Value(targetParams->fps - actualParams->fps);
                    return Value();
                },
                nx::vms::server::metrics::timerWatch<Camera*>(kFpsDeltaCheckInterval)
            )
        );
}

auto makeAnalyticsGroupProvider()
{
    return
        utils::metrics::makeValueGroupProvider<Resource>(
            "analytics",
            utils::metrics::makeLocalValueProvider<Resource>(
                "archiveLengthS",
                [](const auto& r)
                {
                    const auto value = duration_cast<seconds>(r->nxOccupiedDuration());
                    return value.count() > 0 ? Value((double) value.count()) : Value();
                }
            ),
            utils::metrics::makeSystemValueProvider<Resource>(
                "minArchiveLengthS",
                [](const auto& r)
                {
                    if (const auto days = r->minDays(); days > 0 && r->isLicenseUsed())
                        return Value((double) duration_cast<seconds>(hours(days * 24)).count());
                    return Value();
                }
            ),
            utils::metrics::makeLocalValueProvider<Resource>(
                "recordingBitrateBps",
                [](const auto& r) { return Value(r->recordingBitrateKBps(kBitratePeriod) * 1024); }
            )
        );
}

utils::metrics::ValueGroupProviders<CameraController::Resource> CameraController::makeProviders()
{
    return nx::utils::make_container<utils::metrics::ValueGroupProviders<Resource>>(
        makeInfoGroupProvider(),
        makeAvailabilityGroupProvider(),
        makePrimaryStreamGroupProvider(),
        makeSecondaryStreamGroupProvider(),
        makeAnalyticsGroupProvider()
    );
}

} // namespace nx::vms::server::metrics
