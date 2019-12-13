#include "camera_controller.h"

#include <nx/fusion/serialization/lexical.h>
#include <core/resource/resource_fwd.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/text/archive_duration.h>
#include <core/resource/media_server_resource.h>

#include "helpers.h"

namespace nx::vms::server::metrics {

using namespace nx::vms::api;
using namespace nx::vms::server::resource;
using namespace nx::vms::text;

using Resource = CameraController::Resource;
using Value = CameraController::Value;

static const std::chrono::minutes kIssuesRateUpdateInterval(1);
static const std::chrono::seconds kipConflictsRateUpdateInterval(15);
static std::chrono::minutes kBitratePeriod(5);
static const std::chrono::seconds kFpsDeltaCheckInterval(5);

CameraController::CameraController(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule),
    utils::metrics::ResourceControllerImpl<resource::Camera*>("cameras", makeProviders())
{
}

void CameraController::start()
{
    const auto resourcePool = serverModule()->commonModule()->resourcePool();
    QObject::connect(
        resourcePool, &QnResourcePool::resourceAdded,
        [this](const QnResourcePtr& resource)
        {
            if (auto camera = resource.dynamicCast<resource::Camera>().get())
            {
                if (camera->hasFlags(Qn::desktop_camera))
                    return; //< Ignore desktop cameras.

                const auto addOrUpdate = [this, camera]() { add(camera, moduleGUID()); };
                QObject::connect(camera, &QnResource::parentIdChanged, addOrUpdate);
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

namespace {

auto makeInfoProviders()
{
    return nx::utils::make_container<utils::metrics::ValueProviders<Resource>>(
        utils::metrics::makeSystemValueProvider<Resource>(
            "server",
            [](const auto& r) { return Value(r->getParentId().toSimpleString()); }
        ),
        utils::metrics::makeSystemValueProvider<Resource>(
            "type",
            [](const auto& r) { return Value(QnLexical::serialized(r->deviceType())); }
        ),
        utils::metrics::makeSystemValueProvider<Resource>(
            "ip",
            [](const auto& r) { return Value(r->getHostAddress()); }
        ),
        utils::metrics::makeSystemValueProvider<Resource>(
            "vendor",
            [](const auto& r) { return Value(r->getVendor()); }
        ),
        utils::metrics::makeSystemValueProvider<Resource>(
            "model",
            [](const auto& r) { return Value(r->getModel()); }
        ),
        utils::metrics::makeSystemValueProvider<Resource>(
            "firmware",
            [](const auto& r) { return Value(r->getFirmware()); }
        ),
        utils::metrics::makeSystemValueProvider<Resource>(
            "recording",
            [](const auto& r) { return Value(QnLexical::serialized(r->recordingState())); }
        )
    );
}

auto makeAvailabilityProviders()
{
    return nx::utils::make_container<utils::metrics::ValueProviders<Resource>>(
        utils::metrics::makeSystemValueProvider<Resource>(
            "offlineFromOnlineStatus",
            [](const auto& r)
            {
                const auto status = r->getStatus();
                if (status == Qn::Offline && QnResource::isOnline(r->getPreviousStatus()))
                    return Value(QnLexical::serialized(status));
                return Value();
            },
            qtSignalWatch<Resource>(&resource::Camera::statusChanged)
        ),
        utils::metrics::makeSystemValueProvider<Resource>(
            "status",
            [](const auto& r)
            {
                const auto status = r->getStatus();
                if (status == Qn::Offline && isServerOffline(r))
                    return Value("Server Offline");
                return Value(QnLexical::serialized(status));
            }
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "streamIssues",
            [](const auto& r) { return Value(r->getMetric(&Camera::Metrics::streamIssues)); },
            timerWatch<Camera*>(kIssuesRateUpdateInterval)
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "ipConflicts",
            [](const auto& r) { return Value(r->getMetric(&Camera::Metrics::ipConflicts)); },
            timerWatch<Camera*>(kipConflictsRateUpdateInterval)
        )
    );
}

static bool isCameraControlEnabled(const Resource& camera)
{
    return !(
        camera->getCameraCapabilities().testFlag(Qn::FixedQualityCapability)
        || camera->isCameraControlDisabled()
        || !camera->hasVideo()
    );

}

auto makeStreamProviders(StreamIndex streamIndex)
{
    return nx::utils::make_container<utils::metrics::ValueProviders<Resource>>(
        utils::metrics::makeLocalValueProvider<Resource>(
            "resolution",
            [streamIndex](const auto& r)
            {
                auto p = r->actualParams(streamIndex);
                if (!p || !p->resolution.isValid())
                    p = r->targetParams(streamIndex);
                if (p && p->resolution.isValid())
                {
                    auto resolution = p->resolution;
                    if (auto layout = r->getVideoLayout())
                    {
                        resolution.setWidth(resolution.width() * layout->size().width());
                        resolution.setHeight(resolution.height() * layout->size().height());
                    }
                    return Value(CameraMediaStreamInfo::resolutionToString(resolution));
                }
                return Value();
            }
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "targetFps",
            [streamIndex](const auto& r)
            {
                if (auto p = r->targetParams(streamIndex); isCameraControlEnabled(r) && p && p->fps > 0)
                    return Value(p->fps);
                return Value();
            }
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "actualFps",
            [streamIndex](const auto& r)
            {
                if (auto p = r->actualParams(streamIndex); p && p->fps > 0)
                    return Value(p->fps);
                return Value();
            }
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "fpsDelta",
            [streamIndex](const auto& r)
            {
                const auto target = r->targetParams(streamIndex).value_or(QnLiveStreamParams{});
                const auto actual = r->actualParams(streamIndex).value_or(QnLiveStreamParams{});
                if (isCameraControlEnabled(r) && target.fps > 0 && actual.fps > 0)
                    return Value(target.fps - actual.fps);
                return Value();
            },
            timerWatch<Camera*>(kFpsDeltaCheckInterval)
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "actualBitrateBps",
            [streamIndex](const auto& r)
            {
                auto params = r->actualParams(streamIndex);
                if (params && params->bitrateKbps > 0)
                    return Value(params->bitrateKbps * 1024 / 8);
                return Value();
            }
        )
    );
}

auto makeStorageProviders()
{
    return nx::utils::make_container<utils::metrics::ValueProviders<Resource>>(
        utils::metrics::makeLocalValueProvider<Resource>(
            "archiveLengthS",
            [](const auto& r)
            {
                using namespace std::chrono;
                const auto value = r->calendarDuration();
                return value.count() > 0 ? Value(duration_cast<seconds>(value)) : Value();
            }
        ),
        utils::metrics::makeSystemValueProvider<Resource>(
            "minArchiveLengthS",
            [](const auto& r)
            {
                if (const auto days = r->minDays(); days > 0 && r->isLicenseUsed())
                    return Value(std::chrono::hours(days * 24));
                return Value();
            }
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "recordingBitrateBps",
            [](const auto& r)
            {
                const auto result = r->recordingBitrateBps(kBitratePeriod);
                return result > 0 ? Value(result) : Value();
            }
        ),
        utils::metrics::makeLocalValueProvider<Resource>(
            "hasArchiveRotated",
            [](const auto& r) { return Value(r->hasArchiveRotated()); }
        )
    );
}

} // namespace

utils::metrics::ValueGroupProviders<CameraController::Resource> CameraController::makeProviders()
{
    return nx::utils::make_container<utils::metrics::ValueGroupProviders<Resource>>(
        utils::metrics::makeValueGroupProvider<Resource>(
            "_",
            utils::metrics::makeSystemValueProvider<Resource>(
                "name", [](const auto& r) { return Value(r->getUserDefinedName()); }
            )
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "info", makeInfoProviders()
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "availability", makeAvailabilityProviders()
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "primaryStream", makeStreamProviders(StreamIndex::primary)
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "secondaryStream", makeStreamProviders(StreamIndex::secondary)
        ),
        utils::metrics::makeValueGroupProvider<Resource>(
            "storage", makeStorageProviders()
        )
    );
}

} // namespace nx::vms::server::metrics
