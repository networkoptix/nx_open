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

    //TODO: #GDM PTZ ptzController->synchronize(PtzPresetsField);

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

    QnPtzPresetList existing;
    int n = widget->ptzController()->getPresets(&existing)
            ? existing.size() + 1
            : 1;

    QnHotkeysHash hotkeys = context()
            ->instance<QnPtzHotkeyKvPairWatcher>()
            ->allHotkeysByResourceId(widget->camera()->getId());

    QList<int> forbiddenHotkeys;
    foreach (const QnPtzPreset &preset, existing) {
        if (!hotkeys.contains(preset.name))
            continue;
        forbiddenHotkeys.push_back(hotkeys[preset.name]);
    }

    QScopedPointer<QnPtzPresetDialog> dialog(new QnPtzPresetDialog(mainWindow()));
    dialog->setForbiddenHotkeys(forbiddenHotkeys);
    dialog->setName(tr("Saved Position %1").arg(n));
    dialog->setWindowTitle(tr("Save Position"));
    if(dialog->exec() != QDialog::Accepted) {
        return;
    }

    //TODO: #GDM PTZ replace if there is a preset with the same name. Maybe ask to replace.

    QString presetId = QUuid::createUuid().toString();
    if (!widget->ptzController()->createPreset(QnPtzPreset(presetId, dialog->name())))
        return;

   /* if(n > 2) {
        QnPtzTour tour;
        tour.name = tr("Tour");
        tour.id = QUuid::createUuid().toString();

        foreach(const QnPtzPreset &preset, existing) {
            QnPtzTourSpot spot;
            spot.presetId = preset.id;
            spot.speed = 0.1;
            spot.stayTime = 0;
            tour.spots.push_back(spot);
        }

        widget->ptzController()->createTour(tour);

        QnSleep::sleep(1);

        widget->ptzController()->activateTour(tour.id);
    }*/

    if (dialog->hotkey() >= 0) {
        hotkeys[presetId] = dialog->hotkey();
        context()->instance<QnPtzHotkeyKvPairWatcher>()->updateHotkeys(widget->camera()->getId(), hotkeys);
    }

#if 0
        QMessageBox::critical(
            mainWindow(),
            tr("Could not get position from camera"),
            tr("An error has occurred while trying to get current position from camera %1.\n\nThe camera is probably in continuous movement mode. Please stop the camera and try again.").arg(camera->getName())
        );
#endif
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

    QScopedPointer<QnPtzPresetsDialog> dialog(new QnPtzPresetsDialog(mainWindow()));
    dialog->setPtzController(widget->ptzController());

    QnPtzHotkeyKvPairWatcher* watcher = context()->instance<QnPtzHotkeyKvPairWatcher>();
    QnHotkeysHash hotkeys = watcher->allHotkeysByResourceId(widget->camera()->getId());
    dialog->setHotkeys(hotkeys);

    if (!dialog->exec())
        return;

    watcher->updateHotkeys(widget->camera()->getId(), dialog->hotkeys());
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

    QScopedPointer<QnPtzToursDialog> dlg(new QnPtzToursDialog(mainWindow()));
    dlg->exec();
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
    if(!controller->getPosition(Qn::DeviceCoordinateSpace, &position))
        return;

    for(int i = 0; i <= 20; i++) {
        position.setZ(i / 20.0);
        controller->absoluteMove(Qn::DeviceCoordinateSpace, position, 1.0);

        QEventLoop loop;
        QTimer::singleShot(10000, &loop, SLOT(quit()));
        loop.exec();

        if(!guard)
            break;

        menu()->trigger(Qn::TakeScreenshotAction, QnActionParameters(widget).withArgument<QString>(Qn::FileNameRole, tr("PTZ_CALIBRATION_%1.jpg").arg(position.z(), 0, 'f', 2)));
    }
}
