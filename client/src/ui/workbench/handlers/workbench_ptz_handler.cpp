#include "workbench_ptz_handler.h"

#include <QtCore/QEventLoop>
#include <QtCore/QTimer>
#include <QtWidgets/QMessageBox>

#include <common/common_globals.h>

#include <core/kvpair/ptz_hotkey_kvpair_watcher.h>
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
    QnSingleCameraPtzHotkeysDelegate(int cameraId, QnWorkbenchContext *context):
        QnWorkbenchContextAware(context),
        m_cameraId(cameraId){}
    ~QnSingleCameraPtzHotkeysDelegate() {}

    virtual QnHotkeysHash hotkeys() const override {
        return context()->instance<QnPtzHotkeyKvPairWatcher>()->allHotkeysByResourceId(m_cameraId);
    }

    virtual void updateHotkeys(const QnHotkeysHash &value) override {
        context()->instance<QnPtzHotkeyKvPairWatcher>()->updateHotkeys(m_cameraId, value);
    }
private:
    int m_cameraId;
};


QnWorkbenchPtzHandler::QnWorkbenchPtzHandler(QObject *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(Qn::PtzSavePresetAction),                    SIGNAL(triggered()),    this,   SLOT(at_ptzSavePresetAction_triggered()));
    connect(action(Qn::PtzGoToPresetAction),                    SIGNAL(triggered()),    this,   SLOT(at_ptzGoToPresetAction_triggered()));
    connect(action(Qn::PtzManagePresetsAction),                 SIGNAL(triggered()),    this,   SLOT(at_ptzManagePresetsAction_triggered()));
    connect(action(Qn::PtzStartTourAction),                     SIGNAL(triggered()),    this,   SLOT(at_ptzStartTourAction_triggered()));
    connect(action(Qn::PtzManageToursAction),                   SIGNAL(triggered()),    this,   SLOT(at_ptzManageToursAction_triggered()));
    connect(action(Qn::DebugCalibratePtzAction),                SIGNAL(triggered()),    this,   SLOT(at_debugCalibratePtzAction_triggered()));
}

void QnWorkbenchPtzHandler::at_ptzSavePresetAction_triggered() {
    QnMediaResourceWidget* widget = menu()->currentParameters(sender()).mediaWidget();
    if(!widget || !widget->ptzController() || !widget->camera())
        return;

    //TODO: #GDM PTZ fix the text
    if(widget->camera()->getStatus() == QnResource::Offline || widget->camera()->getStatus() == QnResource::Unauthorized) {
        QMessageBox::critical(
            mainWindow(),
            tr("Could not get position from camera"),
            tr("An error has occurred while trying to get current position from camera %1.\n\n"\
               "Please wait for the camera to go online.").arg(widget->camera()->getName())
        );
        return;
    }

    QScopedPointer<QnSingleCameraPtzHotkeysDelegate> hotkeysDelegate(new QnSingleCameraPtzHotkeysDelegate(widget->camera()->getId(), context()));

    QScopedPointer<QnPtzPresetDialog> dialog(new QnPtzPresetDialog(mainWindow()));
    dialog->setHotkeysDelegate(hotkeysDelegate.data());
    dialog->setPtzController(widget->ptzController());
    dialog->exec();
}

void QnWorkbenchPtzHandler::at_ptzGoToPresetAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnMediaResourceWidget *widget = parameters.mediaWidget();
    QString id = parameters.argument<QString>(Qn::PtzPresetIdRole).trimmed();

    if(!widget || !widget->ptzController() || !widget->camera() || id.isEmpty())
        return;

    qDebug() << "goToPreset activated" << widget->camera()->getId() << id;

    if (widget->ptzController()->activatePreset(id, 1.0)) {
        action(Qn::JumpToLiveAction)->trigger(); // TODO: #Elric ?
    } else {
        if(widget->camera()->getStatus() == QnResource::Offline ||
                widget->camera()->getStatus() == QnResource::Unauthorized) {
            QMessageBox::critical(
                mainWindow(),
                tr("Could not set position to camera"),
                tr("An error has occurred while trying to set current position for camera %1.\n\n"\
                   "Please wait for the camera to go online.").arg(widget->camera()->getName())
            );
            return;
        }
        //TODO: #GDM PTZ check other cases
    }
}

void QnWorkbenchPtzHandler::at_ptzManagePresetsAction_triggered() {
    QnMediaResourceWidget *widget = menu()->currentParameters(sender()).mediaWidget();
    if(!widget || !widget->ptzController() || !widget->camera())
        return;

    QScopedPointer<QnSingleCameraPtzHotkeysDelegate> hotkeysDelegate(new QnSingleCameraPtzHotkeysDelegate(widget->camera()->getId(), context()));

    QScopedPointer<QnPtzPresetsDialog> dialog(new QnPtzPresetsDialog(mainWindow()));
    dialog->setHotkeysDelegate(hotkeysDelegate.data());
    dialog->setPtzController(widget->ptzController());
    dialog->exec();
}

void QnWorkbenchPtzHandler::at_ptzStartTourAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnMediaResourceWidget *widget = parameters.mediaWidget();

    if(!widget || !widget->ptzController() || !widget->camera())
        return;

    QString id = parameters.argument<QString>(Qn::PtzTourIdRole).trimmed();
    if(id.isEmpty())
        return;

    qDebug() << "startTour activated" << widget->camera()->getId() << id;

    if (widget->ptzController()->activateTour(id)) {
        action(Qn::JumpToLiveAction)->trigger(); // TODO: #Elric ?
    } else {
        if(widget->camera()->getStatus() == QnResource::Offline ||
                widget->camera()->getStatus() == QnResource::Unauthorized) {
            QMessageBox::critical(
                mainWindow(),
                tr("Could not set position to camera"),
                tr("An error has occurred while trying to set current position for camera %1.\n\n"\
                   "Please wait for the camera to go online.").arg(widget->camera()->getName())
            );
            return;
        }
        //TODO: #GDM PTZ check other cases
    }
}

void QnWorkbenchPtzHandler::at_ptzManageToursAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnMediaResourceWidget *widget = parameters.mediaWidget();
    QnPtzTourList tours;

    if(!widget || !widget->ptzController() || !widget->camera() || !widget->ptzController()->getTours(&tours))
        return;

    QScopedPointer<QnPtzToursDialog> dialog(new QnPtzToursDialog(mainWindow()));
    dialog->setPtzController(widget->ptzController());
    dialog->exec();
}

void QnWorkbenchPtzHandler::at_debugCalibratePtzAction_triggered() {
    QnMediaResourceWidget *widget = menu()->currentParameters(sender()).mediaWidget();
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
