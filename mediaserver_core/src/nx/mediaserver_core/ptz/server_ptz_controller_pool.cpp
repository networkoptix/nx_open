#include "server_ptz_controller_pool.h"

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <core/resource/param.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/ptz_mapper.h>

#include <nx/mediaserver/resource/camera.h>
#include <nx/mediaserver_core/ptz/ptz_ini_config.h>
#include <nx/mediaserver_core/ptz/server_ptz_helpers.h>

namespace core_ptz = nx::core::ptz;
namespace mediaserver_ptz = nx::mediaserver_core::ptz;

namespace {

core_ptz::RelativeContinuousMoveMapping relativeMoveMapping(const QnResourcePtr& resource)
{
    static const QString kRelativeMoveMapping("relativeMoveMapping");
    const auto camera = resource.dynamicCast<QnSecurityCamResource>();
    if (!camera)
        return core_ptz::RelativeContinuousMoveMapping();

    const auto resourceData = qnStaticCommon->dataPool()->data(camera);
    return resourceData.value<core_ptz::RelativeContinuousMoveMapping>(
        kRelativeMoveMapping, core_ptz::RelativeContinuousMoveMapping());
}

QnPtzMapperPtr mapper(const QnSecurityCamResourcePtr& camera)
{
    if (!camera)
    {
        NX_ASSERT(false, "No camera provided");
        return QnPtzMapperPtr();
    }

    return qnStaticCommon
        ->dataPool()
        ->data(camera)
        .value<QnPtzMapperPtr>(lit("ptzMapper"));
}

} // namespace

QnServerPtzControllerPool::QnServerPtzControllerPool(QObject *parent):
    base_type(parent)
{
    NX_DEBUG(this, "Creating server PTZ controller pool");
    setConstructionMode(ThreadedControllerConstruction);

    connect(
        this, &QnServerPtzControllerPool::controllerAboutToBeChanged,
        this, &QnServerPtzControllerPool::at_controllerAboutToBeChanged);

    connect(
        this, &QnServerPtzControllerPool::controllerChanged,
        this, &QnServerPtzControllerPool::at_controllerChanged);
}

QnServerPtzControllerPool::~QnServerPtzControllerPool()
{
    disconnect(this, nullptr, this, nullptr);
    deinitialize();
}

void QnServerPtzControllerPool::registerResource(const QnResourcePtr &resource)
{
    // TODO: #Elric we're creating controller from main thread.
    // Controller ctor may take some time (several seconds).
    // => main thread will stall.

    NX_DEBUG(this, lm("Registering resource %1 (%2)")
        .args(resource->getName(), resource->getId()));

    connect(
        resource, &QnResource::initializedChanged,
        this, &QnServerPtzControllerPool::updateController,
        Qt::QueuedConnection);

    base_type::registerResource(resource);
}

void QnServerPtzControllerPool::unregisterResource(const QnResourcePtr &resource)
{
    NX_DEBUG(this, lm("Unregistering resource %1 (%2)")
        .args(resource->getName(), resource->getId()));

    base_type::unregisterResource(resource);
    disconnect(resource, nullptr, this, nullptr);
}

QnPtzControllerPtr QnServerPtzControllerPool::createController(
    const QnResourcePtr &resource) const
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

    auto camera = resource.dynamicCast<nx::mediaserver::resource::Camera>();
    if (!camera)
    {
        NX_DEBUG(
            this,
            lm("Resource %1 (%2) is not a descendant of "
                "nx::mediaserver::resource::Camera, do nothing.")
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

    bool disableNativePresets = false;
    if (mediaserver_ptz::capabilities(controller).testFlag(Ptz::NativePresetsPtzCapability))
    {
        disableNativePresets = !resource->getProperty(
            Qn::DISABLE_NATIVE_PTZ_PRESETS_PARAM_NAME).isEmpty();

        connect(
            resource, &QnResource::propertyChanged,
            this, &QnServerPtzControllerPool::at_cameraPropertyChanged,
            Qt::UniqueConnection);
    }

    mediaserver_ptz::ControllerWrappingParameters wrappingParameters;
    wrappingParameters.capabilitiesToAdd = mediaserver_ptz::ini().addedCapabilities();
    wrappingParameters.capabilitiesToRemove = mediaserver_ptz::ini().excludedCapabilities();
    wrappingParameters.absoluteMoveMapper = mapper(camera);
    wrappingParameters.relativeMoveMapping = relativeMoveMapping(camera);
    wrappingParameters.areNativePresetsDisabled = disableNativePresets;
    wrappingParameters.ptzPool = qnPtzPool;
    wrappingParameters.commonModule = commonModule();

    NX_DEBUG(
        this,
        lm("Wrapping PTZ controller for resource %1 (%2). "
           "Parameters: %3")
            .args(resource->getName(), resource->getId(), wrappingParameters));

    controller = mediaserver_ptz::wrapController(controller, wrappingParameters);
    const auto controllerCapabilities = mediaserver_ptz::capabilities(controller);
    if (cameraPtzCapabilities != controllerCapabilities)
    {
        NX_DEBUG(
            this,
            lm("Updating PTZ capabilities for resource %1 (%2), setting them to %3")
                .args(camera->getName(), camera->getId(), controllerCapabilities));

        camera->setPtzCapabilities(controllerCapabilities);
        camera->saveParamsAsync();
    }

    return controller;
}

void QnServerPtzControllerPool::at_cameraPropertyChanged(
    const QnResourcePtr& resource, const QString& key)
{
    if (key == Qn::DISABLE_NATIVE_PTZ_PRESETS_PARAM_NAME)
    {
        // Camera reinitialization is required.
        NX_DEBUG(
            this,
            lm("Native presets support has been changed for the resource %1 (%2). "
               "Reinitializing")
                .args(resource->getName(), resource->getId()));

        resource->setStatus(Qn::Offline);
    }
}

void QnServerPtzControllerPool::at_controllerAboutToBeChanged(const QnResourcePtr &resource)
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

void QnServerPtzControllerPool::at_controllerChanged(const QnResourcePtr &resource)
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
