#include "server_ptz_controller_pool.h"

#include <common/common_module.h>

#include <core/resource/param.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/ptz_mapper.h>

#include <nx/core/ptz/relative/relative_continuous_move_mapping.h>

#include <nx/vms/server/resource/camera.h>
#include <nx/vms/server/ptz/ptz_ini_config.h>
#include <nx/vms/server/ptz/server_ptz_helpers.h>

#include <nx/utils/log/log.h>

namespace core_ptz = nx::core::ptz;

namespace {

core_ptz::RelativeContinuousMoveMapping relativeMoveMapping(const QnResourcePtr& resource)
{
    static const QString kRelativeMoveMapping("relativeMoveMapping");
    static const QString kSimpleRelativeMoveMapping("simpleRelativeMoveMapping");

    const auto camera = resource.dynamicCast<QnSecurityCamResource>();
    if (!camera)
        return core_ptz::RelativeContinuousMoveMapping();

    const auto resourceData = camera->resourceData();

    if (resourceData.contains(kRelativeMoveMapping))
    {
        return resourceData.value<core_ptz::RelativeContinuousMoveMapping>(
            kRelativeMoveMapping,
            core_ptz::RelativeContinuousMoveMapping());
    }

    if (resourceData.contains(kSimpleRelativeMoveMapping))
    {
        const auto simpleMapping =
            resourceData.value<core_ptz::SimpleRelativeContinuousMoveMapping>(
                kRelativeMoveMapping,
                core_ptz::SimpleRelativeContinuousMoveMapping());

        return core_ptz::RelativeContinuousMoveMapping(simpleMapping);
    }

    return core_ptz::RelativeContinuousMoveMapping();
}

QnPtzMapperPtr mapper(const QnSecurityCamResourcePtr& camera)
{
    if (!camera)
    {
        NX_ASSERT(false, "No camera provided");
        return QnPtzMapperPtr();
    }

    return camera->resourceData().value<QnPtzMapperPtr>(lit("ptzMapper"));
}

} // namespace

namespace nx {
namespace vms::server {
namespace ptz {

ServerPtzControllerPool::ServerPtzControllerPool(QObject* parent):
    base_type(parent)
{
    NX_DEBUG(this, "Creating server PTZ controller pool");
    setConstructionMode(ThreadedControllerConstruction);

    connect(
        this, &ServerPtzControllerPool::controllerAboutToBeChanged,
        this, &ServerPtzControllerPool::at_controllerAboutToBeChanged);

    connect(
        this, &ServerPtzControllerPool::controllerChanged,
        this, &ServerPtzControllerPool::at_controllerChanged);

    ini().reload();
}

ServerPtzControllerPool::~ServerPtzControllerPool()
{
    disconnect(this, nullptr, this, nullptr);
    deinitialize();
}

void ServerPtzControllerPool::registerResource(const QnResourcePtr& resource)
{
    // TODO: #Elric we're creating controller from main thread.
    // Controller ctor may take some time (several seconds).
    // => main thread will stall.

    NX_DEBUG(this, lm("Registering resource %1 (%2)")
        .args(resource->getName(), resource->getId()));

    connect(
        resource, &QnResource::initializedChanged,
        this, &ServerPtzControllerPool::updateController,
        Qt::QueuedConnection);

    base_type::registerResource(resource);
}

void ServerPtzControllerPool::unregisterResource(const QnResourcePtr& resource)
{
    NX_DEBUG(this, lm("Unregistering resource %1 (%2)")
        .args(resource->getName(), resource->getId()));

    base_type::unregisterResource(resource);
    resource->disconnect(this);
}

QnPtzControllerPtr ServerPtzControllerPool::createController(
    const QnResourcePtr& resource) const
{
    NX_DEBUG(this, lm("Attempting to create PTZ controller for resource %1 (%2)")
        .args(resource->getName(), resource->getId()));

    if (resource->flags() & Qn::foreigner)
    {
        NX_DEBUG(this, lm("Resource %1 (%2) belongs to another server, do nothing.")
            .args(resource->getName(), resource->getId()));
        return QnPtzControllerPtr();
    }

    if (!resource->isInitialized())
    {
        NX_DEBUG(this, lm("Resource %1 (%2) is not initialized, do nothing.")
            .args(resource->getName(), resource->getId()));
        return QnPtzControllerPtr();
    }

    auto camera = resource.dynamicCast<nx::vms::server::resource::Camera>();
    if (!camera)
    {
        NX_DEBUG(
            this,
            lm("Resource %1 (%2) is not a descendant of "
                "nx::vms::server::resource::Camera, do nothing.")
                .args(resource->getName(), resource->getId()));

        return QnPtzControllerPtr();
    }

    const auto cameraPtzCapabilities = camera->getPtzCapabilities();
    QnPtzControllerPtr controller(camera->createPtzController());
    if (!controller)
    {
        NX_DEBUG(
            this,
            lm("Resource %1 (%2) created an empty ptz controller, "
                "resetting resource PTZ capabilities.")
                .args(resource->getName(), resource->getId()));

        if (cameraPtzCapabilities != Ptz::NoPtzCapabilities)
            camera->setPtzCapabilities(Ptz::NoPtzCapabilities);

        return QnPtzControllerPtr();
    }

    bool preferSystemPresets = false;
    const auto capabilities = ptz::capabilities(controller);
    if (capabilities.testFlag(Ptz::NativePresetsPtzCapability)
        && !capabilities.testFlag(Ptz::NoNxPresetsPtzCapability)
        || camera->ptzCapabilitiesUserIsAllowedToModify() != Ptz::Capability::NoPtzCapabilities)
    {
        preferSystemPresets = camera->preferredPtzPresetType() == nx::core::ptz::PresetType::system;
        connect(
            camera.get(), &QnSecurityCamResource::ptzConfigurationChanged,
            this, &ServerPtzControllerPool::at_ptzConfigurationChanged,
            (Qt::ConnectionType) (Qt::DirectConnection | Qt::UniqueConnection));
    }

    ptz::ControllerWrappingParameters wrappingParameters;
    wrappingParameters.capabilitiesToAdd = ptz::ini().addedCapabilities();
    wrappingParameters.capabilitiesToRemove = ptz::ini().excludedCapabilities();
    wrappingParameters.absoluteMoveMapper = mapper(camera);
    wrappingParameters.relativeMoveMapping = relativeMoveMapping(camera);
    wrappingParameters.preferSystemPresets = preferSystemPresets;
    wrappingParameters.ptzPool = this;
    wrappingParameters.commonModule = commonModule();

    NX_DEBUG(
        this,
        lm("Wrapping PTZ controller for resource %1 (%2). "
           "Parameters: %3")
            .args(resource->getName(), resource->getId(), wrappingParameters));

    controller = ptz::wrapController(controller, wrappingParameters);
    const auto controllerCapabilities = ptz::capabilities(controller);
    if (cameraPtzCapabilities != controllerCapabilities)
    {
        NX_DEBUG(
            this,
            lm("Updating PTZ capabilities for resource %1 (%2), setting them to %3")
                .args(camera->getName(), camera->getId(), controllerCapabilities));

        camera->setPtzCapabilities(controllerCapabilities);
        camera->savePropertiesAsync();
    }

    return controller;
}

void ServerPtzControllerPool::at_ptzConfigurationChanged(const QnResourcePtr &resource)
{
    if (!resource->isInitialized() || resource->isInitializationInProgress())
        return;

    NX_DEBUG(this, "PTZ configuration changed for resource %1 (%2), initiate reinitialization",
        resource->getName(), resource->getId());

    resource->reinitAsync();
}

void ServerPtzControllerPool::at_controllerAboutToBeChanged(const QnResourcePtr &resource)
{
    QnPtzControllerPtr oldController = controller(resource);
    if (oldController)
    {
        QnPtzObject object;
        const bool hasValidActiveObject = oldController->getActiveObject(&object)
            && object.type != Qn::InvalidPtzObject;

        if (hasValidActiveObject)
        {
            QnMutexLocker lock(&m_mutex);
            activeObjectByResource[resource] = object;
        }
    }
}

void ServerPtzControllerPool::at_controllerChanged(const QnResourcePtr& resource)
{
    QnPtzControllerPtr controller = this->controller(resource);
    if(!controller)
        return;

    QnPtzObject object;
    {
        QnMutexLocker lock(&m_mutex);
        object = activeObjectByResource.take(resource);
    }

    if (object.type != Qn::TourPtzObject)
        controller->getHomeObject(&object);

    if(object.type == Qn::TourPtzObject)
    {
        NX_DEBUG(this, lm("Activating tour %1 for %2 (%3)")
            .args(object.id, resource->getName(), resource->getId()));
        controller->activateTour(object.id);
    }
    else if(object.type == Qn::PresetPtzObject)
    {
        NX_DEBUG(this, lm("Activating preset %1 for %2 (%3)")
            .args(object.id, resource->getName(), resource->getId()));
        controller->activatePreset(object.id, QnAbstractPtzController::MaxPtzSpeed);
    }
}

} // namespace ptz
} // namespace vms::server
} // namespace nx
