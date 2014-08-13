#include "workbench_ptz_handler.h"

#include <QtCore/QEventLoop>
#include <QtCore/QTimer>
#include <QtWidgets/QMessageBox>

#include <api/app_server_connection.h>

#include <utils/common/collection.h>
#include <utils/resource_property_adaptors.h>

#include <common/common_globals.h>

#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/ptz_preset.h>
#include <core/ptz/ptz_tour.h>
#include <core/resource/camera_resource.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/actions/action_target_provider.h>

#include <ui/dialogs/ptz_preset_dialog.h>
#include <ui/dialogs/ptz_manage_dialog.h>

#include <ui/graphics/items/resource/media_resource_widget.h>

#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_context.h>

class QnSingleCameraPtzHotkeysDelegate: public QnAbstractPtzHotkeyDelegate, public QnWorkbenchContextAware {
public:
    QnSingleCameraPtzHotkeysDelegate(const QnResourcePtr &resource, QnWorkbenchContext *context):
        QnWorkbenchContextAware(context),
        m_resourceId(resource->getId()),
        m_adaptor(new QnPtzHotkeysResourcePropertyAdaptor())
    {
        m_adaptor->setResource(resource);
    }

    ~QnSingleCameraPtzHotkeysDelegate() {}

    virtual QnPtzHotkeyHash hotkeys() const override {
        return m_adaptor->value();
    }

    virtual void updateHotkeys(const QnPtzHotkeyHash &value) override {
        m_adaptor->setValue(value);
    }

private:
    QUuid m_resourceId;
    QScopedPointer<QnPtzHotkeysResourcePropertyAdaptor> m_adaptor;
};


bool getDevicePosition(const QnPtzControllerPtr &controller, QVector3D *position) {
    if(!controller->hasCapabilities(Qn::AsynchronousPtzCapability)) {
        return controller->getPosition(Qn::DevicePtzCoordinateSpace, position);
    } else {
        QEventLoop eventLoop;
        bool result = true;

        QMetaObject::Connection connection = QObject::connect(controller.data(), &QnAbstractPtzController::finished, &eventLoop, [&](Qn::PtzCommand command, const QVariant &data) {
            if(command == Qn::GetDevicePositionPtzCommand) {
                result = data.isValid();
                if(result)
                    *position = data.value<QVector3D>();
                eventLoop.exit();
            }
        }, Qt::QueuedConnection);

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
    connect(action(Qn::PtzSavePresetAction),                    &QAction::triggered,    this,   &QnWorkbenchPtzHandler::at_ptzSavePresetAction_triggered);
    connect(action(Qn::PtzActivatePresetAction),                &QAction::triggered,    this,   &QnWorkbenchPtzHandler::at_ptzActivatePresetAction_triggered);
    connect(action(Qn::PtzActivateTourAction),                  &QAction::triggered,    this,   &QnWorkbenchPtzHandler::at_ptzActivateTourAction_triggered);
    connect(action(Qn::PtzActivateObjectAction),                &QAction::triggered,    this,   &QnWorkbenchPtzHandler::at_ptzActivateObjectAction_triggered);
    connect(action(Qn::PtzManageAction),                        &QAction::triggered,    this,   &QnWorkbenchPtzHandler::at_ptzManageAction_triggered);
    connect(action(Qn::DebugCalibratePtzAction),                &QAction::triggered,    this,   &QnWorkbenchPtzHandler::at_debugCalibratePtzAction_triggered);
    connect(action(Qn::DebugGetPtzPositionAction),              &QAction::triggered,    this,   &QnWorkbenchPtzHandler::at_debugGetPtzPositionAction_triggered);
}

QnWorkbenchPtzHandler::~QnWorkbenchPtzHandler() {
    delete QnPtzManageDialog::instance();
}

void QnWorkbenchPtzHandler::at_ptzSavePresetAction_triggered() {
    QnMediaResourceWidget *widget = menu()->currentParameters(sender()).widget<QnMediaResourceWidget>();
    if(!widget || !widget->ptzController())
        return;
    QnResourcePtr resource = widget->resource()->toResourcePtr();

    //TODO: #GDM #PTZ fix the text
    if(resource->getStatus() == Qn::Offline || resource->getStatus() == Qn::Unauthorized) {
        QMessageBox::critical(
            mainWindow(),
            tr("Could not get position from camera"),
            tr("An error has occurred while trying to get current position from camera %1.\n\n"\
               "Please wait for the camera to go online.").arg(resource->getName())
        );
        return;
    }

    QScopedPointer<QnSingleCameraPtzHotkeysDelegate> hotkeysDelegate(new QnSingleCameraPtzHotkeysDelegate(resource, context()));

    QScopedPointer<QnPtzPresetDialog> dialog(new QnPtzPresetDialog(mainWindow()));
    dialog->setController(widget->ptzController());
    dialog->setHotkeysDelegate(hotkeysDelegate.data());
    dialog->exec();
}

void QnWorkbenchPtzHandler::at_ptzActivatePresetAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnMediaResourceWidget *widget = parameters.widget<QnMediaResourceWidget>();
    QString id = parameters.argument<QString>(Qn::PtzObjectIdRole);

    if(!widget || !widget->ptzController() || id.isEmpty())
        return;
    QnResourcePtr resource = widget->resource()->toResourcePtr();

    if (!widget->ptzController()->hasCapabilities(Qn::PresetsPtzCapability)) {
        //TODO: #GDM #PTZ show appropriate error message?
        return;
    }

    if (widget->dewarpingParams().enabled) {
        QnItemDewarpingParams params = widget->item()->dewarpingParams();
        params.enabled = true;
        widget->item()->setDewarpingParams(params);
    }

    if (widget->ptzController()->activatePreset(id, 1.0)) {
        if (!widget->dewarpingParams().enabled) // do not jump to live if this is a fisheye camera
            action(Qn::JumpToLiveAction)->trigger(); // TODO: #Elric ?
    } else {
        if(resource->getStatus() == Qn::Offline || resource->getStatus() == Qn::Unauthorized) {
            QMessageBox::critical(
                mainWindow(),
                tr("Could not set position for camera"),
                tr("An error has occurred while trying to set current position for camera %1.\n\n"\
                   "Please wait for the camera to go online.").arg(resource->getName())
            );
            return;
        }
        //TODO: #GDM #PTZ check other cases
    }
}

void QnWorkbenchPtzHandler::at_ptzActivateTourAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnMediaResourceWidget *widget = parameters.widget<QnMediaResourceWidget>();
    if(!widget || !widget->ptzController())
        return;
    QnResourcePtr resource = widget->resource()->toResourcePtr();

    QString id = parameters.argument<QString>(Qn::PtzObjectIdRole).trimmed();
    if(id.isEmpty())
        return;

    if (widget->dewarpingParams().enabled) {
        QnItemDewarpingParams params = widget->item()->dewarpingParams();
        params.enabled = true;
        widget->item()->setDewarpingParams(params);
    }

    if (widget->ptzController()->activateTour(id)) {
        if (!widget->dewarpingParams().enabled) // do not jump to live if this is a fisheye camera
            action(Qn::JumpToLiveAction)->trigger(); // TODO: #Elric ?
    } else {
        if(resource->getStatus() == Qn::Offline || resource->getStatus() == Qn::Unauthorized) {
            QMessageBox::critical(
                mainWindow(),
                tr("Could not set position to camera"),
                tr("An error has occurred while trying to set current position for camera %1.\n\n"\
                   "Please wait for the camera to go online.").arg(resource->getName())
            );
            return;
        }
        //TODO: #GDM #PTZ check other cases
    }
}

void QnWorkbenchPtzHandler::at_ptzActivateObjectAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnMediaResourceWidget *widget = parameters.widget<QnMediaResourceWidget>();
    if(!widget || !widget->ptzController())
        return;

    QString id = parameters.argument<QString>(Qn::PtzObjectIdRole).trimmed();
    if(id.isEmpty())
        return;

    QnPtzPresetList presets;
    if(!widget->ptzController()->getPresets(&presets))
        return;

    int index = qnIndexOf(presets, [&](const QnPtzPreset &preset) { return preset.id == id; });
    if(index == -1) {
        menu()->trigger(Qn::PtzActivateTourAction, parameters);
    } else {
        menu()->trigger(Qn::PtzActivatePresetAction, parameters);
    }
}


void QnWorkbenchPtzHandler::at_ptzManageAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnMediaResourceWidget *widget = parameters.widget<QnMediaResourceWidget>();

    if (!widget || !widget->ptzController())
        return;

    QnPtzManageDialog* dialog = QnPtzManageDialog::instance();
    assert(dialog);

    if (dialog->isVisible() && !dialog->tryClose(false))
        return;

    dialog->setController(widget->ptzController());
    dialog->setResource(widget->resource()->toResourcePtr());
    dialog->show();
}

void QnWorkbenchPtzHandler::at_debugCalibratePtzAction_triggered() {
    QnMediaResourceWidget *widget = menu()->currentParameters(sender()).widget<QnMediaResourceWidget>();
    if(!widget)
        return;
    QPointer<QnResourceWidget> guard(widget);
    QnPtzControllerPtr controller = widget->ptzController();

    QVector3D position;
    if(!getDevicePosition(controller, &position))
        return;

    qreal startZ = 0.0;
    qreal endZ = 0.8;

    for(int i = 0; i <= 20; i++) {
        position.setZ(startZ + (endZ - startZ) * i / 20.0);
        controller->absoluteMove(Qn::DevicePtzCoordinateSpace, position, 1.0);

        QEventLoop loop;
        QTimer::singleShot(10000, &loop, SLOT(quit()));
        loop.exec();

        if(!guard)
            break;

        QVector3D cameraPosition;
        getDevicePosition(controller, &cameraPosition);
        qDebug() << "SENT POSITION" << position << "GOT POSITION" << cameraPosition;

        menu()->trigger(Qn::TakeScreenshotAction, QnActionParameters(widget).withArgument<QString>(Qn::FileNameRole, tr("PTZ_CALIBRATION_%1.jpg").arg(position.z(), 0, 'f', 4)));
    }
}

void QnWorkbenchPtzHandler::at_debugGetPtzPositionAction_triggered() {
    QnMediaResourceWidget *widget = menu()->currentParameters(sender()).widget<QnMediaResourceWidget>();
    if(!widget)
        return;
    QnPtzControllerPtr controller = widget->ptzController();

    QVector3D position;
    if(!getDevicePosition(controller, &position)) {
        qDebug() << "COULD NOT GET POSITION";
    } else {
        qDebug() << "GOT POSITION" << position;
    }
}
