#include "server_ptz_controller_pool.h"

#include <api/app_server_connection.h>
#include <common/common_module.h>
#include <common/static_common_module.h>

#include <core/ptz/activity_ptz_controller.h>
#include <core/ptz/home_ptz_controller.h>
#include <core/ptz/mapped_ptz_controller.h>
#include <core/ptz/preset_ptz_controller.h>
#include <core/ptz/ptz_controller_pool.h>
#include <core/ptz/tour_ptz_controller.h>
#include <core/ptz/viewport_ptz_controller.h>
#include <core/ptz/workaround_ptz_controller.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_data_pool.h>
#include <core/resource/param.h>

#include <nx/mediaserver/resource/camera.h>

QnServerPtzControllerPool::QnServerPtzControllerPool(QObject *parent):
    base_type(parent)
{
    setConstructionMode(ThreadedControllerConstruction);
    connect(this, &QnServerPtzControllerPool::controllerAboutToBeChanged, this, &QnServerPtzControllerPool::at_controllerAboutToBeChanged);
    connect(this, &QnServerPtzControllerPool::controllerChanged, this, &QnServerPtzControllerPool::at_controllerChanged);
}

QnServerPtzControllerPool::~QnServerPtzControllerPool()
{
    disconnect( this, nullptr, this, nullptr );
    deinitialize();
}

void QnServerPtzControllerPool::registerResource(const QnResourcePtr& resource)
{
    // TODO: #Elric we're creating controller from main thread.
    // Controller ctor may take some time (several seconds).
    // => main thread will stall.
    connect(resource, &QnResource::initializedChanged, this, &QnServerPtzControllerPool::updateController, Qt::QueuedConnection);
    base_type::registerResource(resource);
}

void QnServerPtzControllerPool::unregisterResource(const QnResourcePtr& resource)
{
    base_type::unregisterResource(resource);
    disconnect(resource, NULL, this, NULL);
}

QnPtzControllerPtr QnServerPtzControllerPool::createController(const QnResourcePtr& resource) const
{
    if(resource->flags() & Qn::foreigner)
        return QnPtzControllerPtr(); /* That's not our resource! */

    if(!resource->isInitialized())
        return QnPtzControllerPtr();

    auto camera = resource.dynamicCast<nx::mediaserver::resource::Camera>();
    if(!camera)
        return QnPtzControllerPtr();

    QnPtzControllerPtr controller(camera->createPtzController());
    if(controller)
    {
        if(QnMappedPtzController::extends(controller->getCapabilities()))
            if(QnPtzMapperPtr mapper = qnStaticCommon->dataPool()->data(camera).value<QnPtzMapperPtr>(lit("ptzMapper")))
                controller.reset(new QnMappedPtzController(mapper, controller));

        if(QnViewportPtzController::extends(controller->getCapabilities()))
            controller.reset(new QnViewportPtzController(controller));

        bool disableNativePresets = false;
        if(controller->getCapabilities().testFlag(Ptz::NativePresetsPtzCapability))
        {
            disableNativePresets = !resource->getProperty(
                Qn::DISABLE_NATIVE_PTZ_PRESETS_PARAM_NAME).isEmpty();

            connect(resource, &QnResource::propertyChanged, this,
                &QnServerPtzControllerPool::at_cameraPropertyChanged, Qt::UniqueConnection);
        }

        if(QnPresetPtzController::extends(controller->getCapabilities(), disableNativePresets))
            controller.reset(new QnPresetPtzController(controller));

        if(QnTourPtzController::extends(controller->getCapabilities()))
            controller.reset(new
                QnTourPtzController(
                    controller,
                    qnPtzPool->commandThreadPool(),
                    qnPtzPool->executorThread()));

        if(QnActivityPtzController::extends(controller->getCapabilities()))
            controller.reset(new QnActivityPtzController(
                commonModule(),
                QnActivityPtzController::Server,
                controller));

        if(QnHomePtzController::extends(controller->getCapabilities()))
            controller.reset(new QnHomePtzController(
                controller,
                qnPtzPool->executorThread()));

        if(QnWorkaroundPtzController::extends(controller->getCapabilities()))
            controller.reset(new QnWorkaroundPtzController(controller));
    }

    Ptz::Capabilities caps = controller ? controller->getCapabilities() : Ptz::NoPtzCapabilities;
    if (camera->getPtzCapabilities() != caps) {
        camera->setPtzCapabilities(caps);
        camera->saveParamsAsync();
    }
    return controller;
}

void QnServerPtzControllerPool::at_addCameraDone(int, ec2::ErrorCode, const QnVirtualCameraResourceList&)
{
}

void QnServerPtzControllerPool::at_cameraPropertyChanged(
    const QnResourcePtr& resource, const QString& key)
{
    if (key == Qn::DISABLE_NATIVE_PTZ_PRESETS_PARAM_NAME)
    {
        // Camera reinitialization is required.
        resource->setStatus(Qn::Offline);
    }
}

void QnServerPtzControllerPool::at_controllerAboutToBeChanged(const QnResourcePtr& resource)
{
    QnPtzControllerPtr oldController = controller(resource);
    if(oldController) {
        QnPtzObject object;
        if(oldController->getActiveObject(&object) && object.type != Qn::InvalidPtzObject) {
            QnMutexLocker lock(&m_mutex);
            activeObjectByResource.insert(resource, object);
        }
    }
}

void QnServerPtzControllerPool::at_controllerChanged(const QnResourcePtr& resource)
{
    QnPtzControllerPtr controller = this->controller(resource);
    if(!controller)
        return;

    QnPtzObject object;
    {
        QnMutexLocker lock(&m_mutex);
        object = activeObjectByResource.take(resource);
    }
    if(object.type != Qn::TourPtzObject)
        controller->getHomeObject(&object);

    if(object.type == Qn::TourPtzObject)
        controller->activateTour(object.id);
    else if(object.type == Qn::PresetPtzObject)
        controller->activatePreset(object.id, QnAbstractPtzController::MaxPtzSpeed);

    qDebug() << "activate" << object.id << object.type;
}
