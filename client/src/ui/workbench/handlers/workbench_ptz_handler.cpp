#include "workbench_ptz_handler.h"

#include <QtCore/QEventLoop>
#include <QtCore/QTimer>
#include <QtWidgets/QMessageBox>

#include <api/app_server_connection.h>

#include <common/common_globals.h>

#include <core/kvpair/ptz_hotkey_kvpair_adapter.h>

#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/ptz_hotkey.h>
#include <core/ptz/ptz_preset.h>
#include <core/ptz/ptz_tour.h>
#include <core/resource/camera_resource.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/actions/action_target_provider.h>

#include <ui/dialogs/ptz_preset_dialog.h>
#include <ui/dialogs/ptz_presets_dialog.h>
#include <ui/dialogs/ptz_tours_dialog.h>

#include <ui/graphics/items/resource/media_resource_widget.h>

#include <ui/workbench/workbench_context.h>

class QnSingleCameraPtzHotkeysDelegate: public QnAbstractPtzHotkeyDelegate, protected QnWorkbenchContextAware {
public:
    QnSingleCameraPtzHotkeysDelegate(const QnResourcePtr &resource, QnWorkbenchContext *context):
        QnWorkbenchContextAware(context),
        m_resourceId(resource->getId()),
        m_adapter(new QnPtzHotkeyKvPairAdapter(resource))
    {}

    ~QnSingleCameraPtzHotkeysDelegate() {}

    virtual QnHotkeysHash hotkeys() const override {
        return m_adapter->hotkeys();
    }

    virtual void updateHotkeys(const QnHotkeysHash &value) override {
        QString serialized = QString::fromUtf8(QJson::serialized(value));

        QnAppServerConnectionFactory::createConnection()->saveAsync(m_resourceId, QnKvPairList() << QnKvPair(m_adapter->key(), serialized));
    }
private:
    int m_resourceId;
    QScopedPointer<QnPtzHotkeyKvPairAdapter> m_adapter;
};


QnWorkbenchPtzHandler::QnWorkbenchPtzHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(Qn::PtzSavePresetAction),                    &QAction::triggered,    this,   &QnWorkbenchPtzHandler::at_ptzSavePresetAction_triggered);
    connect(action(Qn::PtzGoToPresetAction),                    &QAction::triggered,    this,   &QnWorkbenchPtzHandler::at_ptzGoToPresetAction_triggered);
    connect(action(Qn::PtzManagePresetsAction),                 &QAction::triggered,    this,   &QnWorkbenchPtzHandler::at_ptzManagePresetsAction_triggered);
    connect(action(Qn::PtzStartTourAction),                     &QAction::triggered,    this,   &QnWorkbenchPtzHandler::at_ptzStartTourAction_triggered);
    connect(action(Qn::PtzManageToursAction),                   &QAction::triggered,    this,   &QnWorkbenchPtzHandler::at_ptzManageToursAction_triggered);
    connect(action(Qn::DebugCalibratePtzAction),                &QAction::triggered,    this,   &QnWorkbenchPtzHandler::at_debugCalibratePtzAction_triggered);
}

void QnWorkbenchPtzHandler::at_ptzSavePresetAction_triggered() {
    QnMediaResourceWidget *widget = menu()->currentParameters(sender()).widget<QnMediaResourceWidget>();
    if(!widget || !widget->ptzController())
        return;
    QnResourcePtr resource = widget->resource()->toResourcePtr();

    //TODO: #GDM PTZ fix the text
    if(resource->getStatus() == QnResource::Offline || resource->getStatus() == QnResource::Unauthorized) {
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
    dialog->setHotkeysDelegate(hotkeysDelegate.data());
    dialog->setPtzController(widget->ptzController());
    dialog->exec();
}

void QnWorkbenchPtzHandler::at_ptzGoToPresetAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnMediaResourceWidget *widget = parameters.widget<QnMediaResourceWidget>();
    QString id = parameters.argument<QString>(Qn::PtzPresetIdRole).trimmed();

    if(!widget || !widget->ptzController() || id.isEmpty())
        return;
    QnResourcePtr resource = widget->resource()->toResourcePtr();

    if (!widget->ptzController()->hasCapabilities(Qn::PresetsPtzCapability)) {
        //TODO: #GDM PTZ show appropriate error message?
        return;
    }

    qDebug() << "goToPreset activated" << resource->getId() << id;

    if (widget->ptzController()->activatePreset(id, 1.0)) {
        action(Qn::JumpToLiveAction)->trigger(); // TODO: #Elric ?
    } else {
        if(resource->getStatus() == QnResource::Offline || resource->getStatus() == QnResource::Unauthorized) {
            QMessageBox::critical(
                mainWindow(),
                tr("Could not set position for camera"),
                tr("An error has occurred while trying to set current position for camera %1.\n\n"\
                   "Please wait for the camera to go online.").arg(resource->getName())
            );
            return;
        }
        //TODO: #GDM PTZ check other cases
    }
}

void QnWorkbenchPtzHandler::at_ptzManagePresetsAction_triggered() {
    QnMediaResourceWidget *widget = menu()->currentParameters(sender()).widget<QnMediaResourceWidget>();
    if(!widget || !widget->ptzController())
        return;
    QnResourcePtr resource = widget->resource()->toResourcePtr();

    QScopedPointer<QnSingleCameraPtzHotkeysDelegate> hotkeysDelegate(new QnSingleCameraPtzHotkeysDelegate(resource, context()));

    QScopedPointer<QnPtzPresetsDialog> dialog(new QnPtzPresetsDialog(mainWindow()));
    dialog->setHotkeysDelegate(hotkeysDelegate.data());
    dialog->setPtzController(widget->ptzController());
    dialog->exec();
}

void QnWorkbenchPtzHandler::at_ptzStartTourAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnMediaResourceWidget *widget = parameters.widget<QnMediaResourceWidget>();
    if(!widget || !widget->ptzController())
        return;
    QnResourcePtr resource = widget->resource()->toResourcePtr();

    QString id = parameters.argument<QString>(Qn::PtzTourIdRole).trimmed();
    if(id.isEmpty())
        return;

    qDebug() << "startTour activated" << resource->getId() << id;

    if (widget->ptzController()->activateTour(id)) {
        action(Qn::JumpToLiveAction)->trigger(); // TODO: #Elric ?
    } else {
        if(resource->getStatus() == QnResource::Offline || resource->getStatus() == QnResource::Unauthorized) {
            QMessageBox::critical(
                mainWindow(),
                tr("Could not set position to camera"),
                tr("An error has occurred while trying to set current position for camera %1.\n\n"\
                   "Please wait for the camera to go online.").arg(resource->getName())
            );
            return;
        }
        //TODO: #GDM PTZ check other cases
    }
}

void QnWorkbenchPtzHandler::at_ptzManageToursAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnMediaResourceWidget *widget = parameters.widget<QnMediaResourceWidget>();
    QnPtzTourList tours;

    if(!widget || !widget->ptzController() || !widget->ptzController()->getTours(&tours))
        return;

    QScopedPointer<QnPtzToursDialog> dialog(new QnPtzToursDialog(mainWindow()));
    dialog->setPtzController(widget->ptzController());
    dialog->exec();
}

void QnWorkbenchPtzHandler::at_debugCalibratePtzAction_triggered() {
    QnMediaResourceWidget *widget = menu()->currentParameters(sender()).widget<QnMediaResourceWidget>();
    if(!widget)
        return;
    QPointer<QnResourceWidget> guard(widget);

    QnPtzControllerPtr controller = widget->ptzController();
    if(!controller)
        return;

    QVector3D position;
    if(!controller->getPosition(Qn::DevicePtzCoordinateSpace, &position))
        return;

    for(int i = 0; i <= 20; i++) {
        position.setZ(i / 20.0);
        controller->absoluteMove(Qn::DevicePtzCoordinateSpace, position, 1.0);

        QEventLoop loop;
        QTimer::singleShot(10000, &loop, SLOT(quit()));
        loop.exec();

        if(!guard)
            break;

        menu()->trigger(Qn::TakeScreenshotAction, QnActionParameters(widget).withArgument<QString>(Qn::FileNameRole, tr("PTZ_CALIBRATION_%1.jpg").arg(position.z(), 0, 'f', 2)));
    }
}
