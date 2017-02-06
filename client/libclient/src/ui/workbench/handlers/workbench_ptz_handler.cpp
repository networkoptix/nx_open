#include "workbench_ptz_handler.h"

#include <QtCore/QEventLoop>
#include <QtCore/QTimer>

#include <api/app_server_connection.h>

#include <nx/utils/collection.h>
#include <utils/resource_property_adaptors.h>

#include <common/common_globals.h>

#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/ptz_preset.h>
#include <core/ptz/ptz_tour.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource_display_info.h>

#include <client/client_settings.h>

#include <nx/client/messages/ptz_messages.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/actions/action_target_provider.h>
#include <ui/dialogs/ptz_preset_dialog.h>
#include <ui/dialogs/ptz_manage_dialog.h>

#include <ui/graphics/items/resource/media_resource_widget.h>

#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_context.h>

namespace
{
const QString kPtzHotkeysPropertyName = lit("ptzHotkeys");
}

class QnSingleCameraPtzHotkeysDelegate: public QnAbstractPtzHotkeyDelegate, public QnWorkbenchContextAware
{
public:
    QnSingleCameraPtzHotkeysDelegate(const QnResourcePtr &resource, QnWorkbenchContext *context):
        QnWorkbenchContextAware(context),
        m_camera(resource.dynamicCast<QnVirtualCameraResource>()),
        m_resourceId(resource->getId()),
        m_propertyHandler(new QnJsonResourcePropertyHandler<QnPtzHotkeyHash>())
    {
    }

    ~QnSingleCameraPtzHotkeysDelegate() {}

    virtual QnPtzHotkeyHash hotkeys() const override
    {
        QnPtzHotkeyHash result;

        if (!m_camera)
            return result;

        {
            QnMutexLocker lock(&m_mutex);
            m_propertyHandler->deserialize(
                m_camera->getProperty(kPtzHotkeysPropertyName),
                &result);
        }

        return result;
    }

    virtual void updateHotkeys(const QnPtzHotkeyHash &value) override
    {
        if (!m_camera)
            return;

        {
            QnMutexLocker lock(&m_mutex);
            QString serialized;
            m_propertyHandler->serialize(value, &serialized);

            m_camera->setProperty(kPtzHotkeysPropertyName, serialized);

            m_camera->saveParamsAsync();
        }
    }

private:
    QnVirtualCameraResourcePtr m_camera;
    QnUuid m_resourceId;
    std::unique_ptr<QnJsonResourcePropertyHandler<QnPtzHotkeyHash>> m_propertyHandler;
    mutable QnMutex m_mutex;
};


bool getDevicePosition(const QnPtzControllerPtr &controller, QVector3D *position)
{
    if (!controller->hasCapabilities(Qn::AsynchronousPtzCapability))
    {
        return controller->getPosition(Qn::DevicePtzCoordinateSpace, position);
    }
    else
    {
        QEventLoop eventLoop;
        bool result = true;

        auto finishedHandler =
            [&](Qn::PtzCommand command, const QVariant &data)
        {
            if (command == Qn::GetDevicePositionPtzCommand)
            {
                result = data.isValid();
                if (result)
                    *position = data.value<QVector3D>();
                eventLoop.exit();
            }
        };

        QMetaObject::Connection connection = QObject::connect(
            controller.data(),
            &QnAbstractPtzController::finished,
            &eventLoop,
            finishedHandler,
            Qt::QueuedConnection);

        controller->getPosition(Qn::DevicePtzCoordinateSpace, position);
        eventLoop.exec();
        QObject::disconnect(connection);
        return result;
    }
}

QnWorkbenchPtzHandler::QnWorkbenchPtzHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(QnActions::PtzSavePresetAction), &QAction::triggered, this, &QnWorkbenchPtzHandler::at_ptzSavePresetAction_triggered);
    connect(action(QnActions::PtzActivatePresetAction), &QAction::triggered, this, &QnWorkbenchPtzHandler::at_ptzActivatePresetAction_triggered);
    connect(action(QnActions::PtzActivateTourAction), &QAction::triggered, this, &QnWorkbenchPtzHandler::at_ptzActivateTourAction_triggered);
    connect(action(QnActions::PtzActivateObjectAction), &QAction::triggered, this, &QnWorkbenchPtzHandler::at_ptzActivateObjectAction_triggered);
    connect(action(QnActions::PtzManageAction), &QAction::triggered, this, &QnWorkbenchPtzHandler::at_ptzManageAction_triggered);
    connect(action(QnActions::DebugCalibratePtzAction), &QAction::triggered, this, &QnWorkbenchPtzHandler::at_debugCalibratePtzAction_triggered);
    connect(action(QnActions::DebugGetPtzPositionAction), &QAction::triggered, this, &QnWorkbenchPtzHandler::at_debugGetPtzPositionAction_triggered);
}

QnWorkbenchPtzHandler::~QnWorkbenchPtzHandler()
{
    delete QnPtzManageDialog::instance();
}

void QnWorkbenchPtzHandler::at_ptzSavePresetAction_triggered()
{
    QnMediaResourceWidget *widget = menu()->currentParameters(sender()).widget<QnMediaResourceWidget>();
    if (!widget || !widget->ptzController())
        return;
    QnResourcePtr resource = widget->resource()->toResourcePtr();

    //TODO: #GDM #PTZ fix the text
    if (resource->getStatus() == Qn::Offline || resource->getStatus() == Qn::Unauthorized)
    {
        nx::client::messages::Ptz::failedToGetPosition(mainWindow(),
        QnResourceDisplayInfo(resource).toString(qnSettings->extraInfoInTree()));
        return;
    }

    QScopedPointer<QnSingleCameraPtzHotkeysDelegate> hotkeysDelegate(new QnSingleCameraPtzHotkeysDelegate(resource, context()));

    QScopedPointer<QnPtzPresetDialog> dialog(new QnPtzPresetDialog(mainWindow()));
    dialog->setController(widget->ptzController());
    dialog->setHotkeysDelegate(hotkeysDelegate.data());
    dialog->exec();
}

void QnWorkbenchPtzHandler::at_ptzActivatePresetAction_triggered()
{
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnMediaResourceWidget *widget = parameters.widget<QnMediaResourceWidget>();
    QString id = parameters.argument<QString>(Qn::PtzObjectIdRole);

    if (!widget || !widget->ptzController() || id.isEmpty())
        return;
    QnResourcePtr resource = widget->resource()->toResourcePtr();

    if (!widget->ptzController()->hasCapabilities(Qn::PresetsPtzCapability))
    {
        //TODO: #GDM #PTZ show appropriate error message?
        return;
    }

    if (widget->dewarpingParams().enabled)
    {
        QnItemDewarpingParams params = widget->item()->dewarpingParams();
        params.enabled = true;
        widget->item()->setDewarpingParams(params);
    }

    if (widget->ptzController()->activatePreset(id, 1.0))
    {
        if (!widget->dewarpingParams().enabled) // do not jump to live if this is a fisheye camera
            action(QnActions::JumpToLiveAction)->trigger(); // TODO: #Elric ?
    }
    else
    {
        showSetPositionWarning(resource);
    }
}

void QnWorkbenchPtzHandler::at_ptzActivateTourAction_triggered()
{
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnMediaResourceWidget *widget = parameters.widget<QnMediaResourceWidget>();
    if (!widget || !widget->ptzController())
        return;
    QnResourcePtr resource = widget->resource()->toResourcePtr();

    QString id = parameters.argument<QString>(Qn::PtzObjectIdRole).trimmed();
    if (id.isEmpty())
        return;

    /* Check if tour exists and is valid. */
    {
        QnPtzTourList tours;
        if (!widget->ptzController()->getTours(&tours))
            return;

        QnPtzPresetList presets;
        if (!widget->ptzController()->getPresets(&presets))
            return;

        int tourIdx = qnIndexOf(tours, [&id](const QnPtzTour &tour) { return id == tour.id; });
        if (tourIdx < 0)
            return;

        QnPtzTour tour = tours[tourIdx];
        if (!tour.isValid(presets))
            return;
    }

    if (widget->dewarpingParams().enabled)
    {
        QnItemDewarpingParams params = widget->item()->dewarpingParams();
        params.enabled = true;
        widget->item()->setDewarpingParams(params);
    }

    if (widget->ptzController()->activateTour(id))
    {
        if (!widget->dewarpingParams().enabled) // do not jump to live if this is a fisheye camera
            action(QnActions::JumpToLiveAction)->trigger(); // TODO: #Elric ?
    }
    else
    {
        showSetPositionWarning(resource);
    }
}

void QnWorkbenchPtzHandler::at_ptzActivateObjectAction_triggered()
{
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnMediaResourceWidget *widget = parameters.widget<QnMediaResourceWidget>();
    if (!widget || !widget->ptzController())
        return;

    QString id = parameters.argument<QString>(Qn::PtzObjectIdRole).trimmed();
    if (id.isEmpty())
        return;

    QnPtzPresetList presets;
    if (!widget->ptzController()->getPresets(&presets))
        return;

    int index = qnIndexOf(presets, [&](const QnPtzPreset &preset) { return preset.id == id; });
    if (index == -1)
    {
        menu()->trigger(QnActions::PtzActivateTourAction, parameters);
    }
    else
    {
        menu()->trigger(QnActions::PtzActivatePresetAction, parameters);
    }
}


void QnWorkbenchPtzHandler::at_ptzManageAction_triggered()
{
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnMediaResourceWidget *widget = parameters.widget<QnMediaResourceWidget>();

    if (!widget || !widget->ptzController())
        return;

    QnPtzManageDialog* dialog = QnPtzManageDialog::instance();
    if (!dialog)
        dialog = new QnPtzManageDialog(mainWindow());

    if (dialog->isVisible() && !dialog->tryClose(false))
        return;

    auto res = widget->resource()->toResourcePtr();
    auto hotkeysDelegate =
        new QnSingleCameraPtzHotkeysDelegate(res, context());

    dialog->setResource(res);
    dialog->setHotkeysDelegate(hotkeysDelegate);
    dialog->setController(widget->ptzController());
    dialog->show();
}

void QnWorkbenchPtzHandler::at_debugCalibratePtzAction_triggered()
{
    QnMediaResourceWidget *widget = menu()->currentParameters(sender()).widget<QnMediaResourceWidget>();
    if (!widget)
        return;
    QPointer<QnResourceWidget> guard(widget);
    QnPtzControllerPtr controller = widget->ptzController();

    QVector3D position;
    if (!getDevicePosition(controller, &position))
        return;

    qreal startZ = 0.0;
    qreal endZ = 0.8;

    for (int i = 0; i <= 20; i++)
    {
        position.setZ(startZ + (endZ - startZ) * i / 20.0);
        controller->absoluteMove(Qn::DevicePtzCoordinateSpace, position, 1.0);

        QEventLoop loop;
        QTimer::singleShot(10000, &loop, SLOT(quit()));
        loop.exec();

        if (!guard)
            break;

        QVector3D cameraPosition;
        getDevicePosition(controller, &cameraPosition);
        qDebug() << "SENT POSITION" << position << "GOT POSITION" << cameraPosition;

        menu()->trigger(QnActions::TakeScreenshotAction, QnActionParameters(widget)
            .withArgument(Qn::FileNameRole, lit("PTZ_CALIBRATION_%1.jpg").arg(position.z(), 0, 'f', 4)));
    }
}

void QnWorkbenchPtzHandler::at_debugGetPtzPositionAction_triggered()
{
    QnMediaResourceWidget *widget = menu()->currentParameters(sender()).widget<QnMediaResourceWidget>();
    if (!widget)
        return;
    QnPtzControllerPtr controller = widget->ptzController();

    QVector3D position;
    if (!getDevicePosition(controller, &position))
    {
        qDebug() << "COULD NOT GET POSITION";
    }
    else
    {
        qDebug() << "GOT POSITION" << position;
    }
}

void QnWorkbenchPtzHandler::showSetPositionWarning(const QnResourcePtr& resource)
{
    if (resource->getStatus() == Qn::Offline || resource->getStatus() == Qn::Unauthorized)
    {
        nx::client::messages::Ptz::failedToSetPosition(mainWindow(),
            QnResourceDisplayInfo(resource).toString(qnSettings->extraInfoInTree()));
    }
    //TODO: #GDM #PTZ check other cases
}
