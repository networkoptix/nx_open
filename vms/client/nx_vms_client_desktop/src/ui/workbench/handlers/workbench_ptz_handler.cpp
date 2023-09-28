// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_ptz_handler.h"

#include <chrono>

#include <QtCore/QEventLoop>
#include <QtCore/QTimer>
#include <QtGui/QAction>

#include <common/common_globals.h>
#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/ptz_preset.h>
#include <core/ptz/ptz_tour.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource_display_info.h>
#include <nx/vms/client/core/ptz/hotkey_resource_property_adaptor.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/action_parameters.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <nx/vms/client/desktop/ui/messages/ptz_messages.h>
#include <nx/vms/client/desktop/ui/scene/widgets/scene_banners.h>
#include <ui/dialogs/ptz_manage_dialog.h>
#include <ui/dialogs/ptz_preset_dialog.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>

using namespace std::chrono;
using namespace nx::vms::common::ptz;
using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

namespace {

const QString kPtzHotkeysPropertyName = lit("ptzHotkeys");
const seconds kPtzNotSupportedMessageTimeout = 3s;

} // namespace

class QnSingleCameraPtzHotkeysDelegate: public QnAbstractPtzHotkeyDelegate, public QnWorkbenchContextAware
{
public:
    QnSingleCameraPtzHotkeysDelegate(QWidget* parent, const QnResourcePtr &resource, QnWorkbenchContext *context):
        QnWorkbenchContextAware(context),
        m_parent(parent),
        m_camera(resource.dynamicCast<QnVirtualCameraResource>()),
        m_resourceId(resource->getId()),
        m_propertyHandler(new QnJsonResourcePropertyHandler<PresetIdByHotkey>())
    {
    }

    ~QnSingleCameraPtzHotkeysDelegate() {}

    virtual PresetIdByHotkey hotkeys() const override
    {
        PresetIdByHotkey result;

        if (!m_camera)
            return result;

        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            m_propertyHandler->deserialize(
                m_camera->getProperty(kPtzHotkeysPropertyName),
                &result);
        }

        return result;
    }

    virtual void updateHotkeys(const PresetIdByHotkey &value) override
    {
        if (!m_camera)
            return;

        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            QString serialized;
            m_propertyHandler->serialize(value, &serialized);

            m_camera->setProperty(kPtzHotkeysPropertyName, serialized);

            m_camera->savePropertiesAsync();
        }
    }

private:
    QWidget* m_parent;
    QnVirtualCameraResourcePtr m_camera;
    QnUuid m_resourceId;
    std::unique_ptr<QnJsonResourcePropertyHandler<PresetIdByHotkey>> m_propertyHandler;
    mutable nx::Mutex m_mutex;
};

bool getDevicePosition(const QnPtzControllerPtr &controller, Vector* outPosition)
{
    if (!controller->hasCapabilities(Ptz::AsynchronousPtzCapability))
    {
        return controller->getPosition(outPosition, CoordinateSpace::device);
    }
    else
    {
        QEventLoop eventLoop;
        bool result = true;

        auto finishedHandler =
            [&](Command command, const QVariant &data)
            {
                if (command == nx::vms::common::ptz::Command::getDevicePosition)
                {
                    result = data.isValid();
                    if (result)
                        *outPosition = data.value<Vector>();
                    eventLoop.exit();
                }
            };

        QMetaObject::Connection connection = QObject::connect(
            controller.data(),
            &QnAbstractPtzController::finished,
            &eventLoop,
            finishedHandler,
            Qt::QueuedConnection);

        controller->getPosition(outPosition, CoordinateSpace::device);

        eventLoop.exec();
        QObject::disconnect(connection);
        return result;
    }
}

struct QnWorkbenchPtzHandler::Private
{
    QnWorkbenchPtzHandler* q = nullptr;

    QPointer<nx::vms::client::desktop::SceneBanner> banner;

public:
    Private(QnWorkbenchPtzHandler* owner):
        q(owner)
    {
    }

    QVector3D applyRotation(const QVector3D& speed, qreal rotation) const
    {
        QVector3D transformedSpeed = speed;

        rotation = static_cast<qint64>(rotation >= 0 ? rotation + 0.5 : rotation - 0.5) % 360;
        if (rotation < 0)
            rotation += 360;

        if (rotation >= 45 && rotation < 135)
        {
            transformedSpeed.setX(-speed.y());
            transformedSpeed.setY(speed.x());
        }
        else if (rotation >= 135 && rotation < 225)
        {
            transformedSpeed.setX(-speed.x());
            transformedSpeed.setY(-speed.y());
        }
        else if (rotation >= 225 && rotation < 315)
        {
            transformedSpeed.setX(speed.y());
            transformedSpeed.setY(-speed.x());
        }

        return transformedSpeed;
    }

    void showSetPositionWarning(const QnResourcePtr& resource)
    {
        if (resource->getStatus() == nx::vms::api::ResourceStatus::offline
            || resource->getStatus() == nx::vms::api::ResourceStatus::unauthorized)
        {
            messages::Ptz::failedToSetPosition(q->mainWindowWidget(),
                QnResourceDisplayInfo(resource).toString(
                    appContext()->localSettings()->resourceInfoLevel()));
        }
        // TODO: #sivanov Check other cases.
    }

    // Joystick-controlled action must occur only if ptz is active.
    bool checkPtzIsActiveOrForced(QnMediaResourceWidget* widget, bool isSupported)
    {
        const auto parameters = q->menu()->currentParameters(q->sender());

        const bool ptzActive = widget->options().testFlag(QnResourceWidget::ControlPtz);
        const bool ptzForced = parameters.argument(Qn::ItemDataRole::ForceRole).toBool();
        if (!isSupported || (!ptzActive && !ptzForced))
        {
            QnMediaResourcePtr mediaResource = widget->resource();
            if (!mediaResource)
                return false;

            QnResource* resource = mediaResource->toResource();
            if (!resource)
                return false;

            showPtzBanner(resource->getName());

            return false;
        }

        return true;
    }

    bool checkPtzEnabled()
    {
        const auto widget = dynamic_cast<QnMediaResourceWidget*>(q->display()->widget(Qn::CentralRole));
        if (!widget)
            return false;

        return checkPtzIsActiveOrForced(widget, widget->supportsBasicPtz());
    }

    bool checkPtzMoveEnabled()
    {
        const auto widget = dynamic_cast<QnMediaResourceWidget*>(q->display()->widget(Qn::CentralRole));
        if (!widget)
            return false;

        return checkPtzIsActiveOrForced(widget, widget->canControlPtzMove());
    }

    bool checkPtzZoomEnabled()
    {
        const auto widget = dynamic_cast<QnMediaResourceWidget*>(q->display()->widget(Qn::CentralRole));
        if (!widget)
            return false;

        return checkPtzIsActiveOrForced(widget, widget->canControlPtzZoom());
    }

    bool checkPtzFocusEnabled()
    {
        const auto widget = dynamic_cast<QnMediaResourceWidget*>(q->display()->widget(Qn::CentralRole));
        if (!widget)
            return false;

        return checkPtzIsActiveOrForced(widget, widget->canControlPtzFocus());
    }

    void updatePtzState(bool ptzMoveIsActive, QnMediaResourceWidget* widget)
    {
        switch (widget->ptzActivationReason())
        {
            case QnMediaResourceWidget::PtzEnabledBy::joystick:
            {
                widget->setPtzMode(ptzMoveIsActive);
                if (!ptzMoveIsActive)
                    widget->setPtzActivationReason(QnMediaResourceWidget::PtzEnabledBy::nothing);
                break;
            }
            case QnMediaResourceWidget::PtzEnabledBy::nothing:
            {
                if (widget->options().testFlag(QnResourceWidget::ControlPtz))
                {
                    widget->setPtzActivationReason(QnMediaResourceWidget::PtzEnabledBy::user);
                }
                else
                {
                    widget->setPtzActivationReason(QnMediaResourceWidget::PtzEnabledBy::joystick);
                    widget->setPtzMode(ptzMoveIsActive);
                }
                break;
            }
            case QnMediaResourceWidget::PtzEnabledBy::user:
            {
                if (!widget->options().testFlag(QnResourceWidget::ControlPtz))
                    widget->setPtzActivationReason(QnMediaResourceWidget::PtzEnabledBy::nothing);
                break;
            }
            default:
            {
                NX_ASSERT(this, "Unexpected ptz activation reason.");
            }
        }
    }

    void showPtzBanner(const QString& resourceName)
    {
        static const QString kPtzNotSupportedMessageTemplate =
            QnWorkbenchPtzHandler::tr("Camera %1 does not support PTZ");

        if (banner)
            return;

        banner = nx::vms::client::desktop::SceneBanner::show(
            kPtzNotSupportedMessageTemplate.arg(resourceName),
            kPtzNotSupportedMessageTimeout);
    }
}; // QnWorkbenchPtzHandler::Private

QnWorkbenchPtzHandler::QnWorkbenchPtzHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private(this))
{
    connect(action(action::PtzSavePresetAction), &QAction::triggered, this, &QnWorkbenchPtzHandler::at_ptzSavePresetAction_triggered);
    connect(action(action::PtzActivatePresetAction), &QAction::triggered, this, &QnWorkbenchPtzHandler::at_ptzActivatePresetAction_triggered);
    connect(action(action::PtzActivateTourAction), &QAction::triggered, this, &QnWorkbenchPtzHandler::at_ptzActivateTourAction_triggered);
    connect(action(action::PtzActivateObjectAction), &QAction::triggered, this, &QnWorkbenchPtzHandler::at_ptzActivateObjectAction_triggered);
    connect(action(action::PtzManageAction), &QAction::triggered, this, &QnWorkbenchPtzHandler::at_ptzManageAction_triggered);
    connect(action(action::DebugCalibratePtzAction), &QAction::triggered, this, &QnWorkbenchPtzHandler::at_debugCalibratePtzAction_triggered);
    connect(action(action::DebugGetPtzPositionAction), &QAction::triggered, this, &QnWorkbenchPtzHandler::at_debugGetPtzPositionAction_triggered);

    connect(
        action(action::PtzContinuousMoveAction), &QAction::triggered,
        this, &QnWorkbenchPtzHandler::at_ptzContinuousMoveAction_triggered);

    connect(
        action(action::PtzActivatePresetByIndexAction), &QAction::triggered,
        this, &QnWorkbenchPtzHandler::at_ptzActivatePresetByIndexAction_triggered);

    connect(
        action(action::PtzActivateByHotkeyAction), &QAction::triggered,
        this, &QnWorkbenchPtzHandler::at_ptzActivateObjectByHotkeyAction_triggered);

    connect(
        action(action::PtzFocusInAction), &QAction::triggered,
        this, &QnWorkbenchPtzHandler::at_ptzFocusInAction_triggered);
    connect(
        action(action::PtzFocusOutAction), &QAction::triggered,
        this, &QnWorkbenchPtzHandler::at_ptzFocusOutAction_triggered);
    connect(
        action(action::PtzFocusAutoAction), &QAction::triggered,
        this, &QnWorkbenchPtzHandler::at_ptzFocusAutoAction_triggered);
}

QnWorkbenchPtzHandler::~QnWorkbenchPtzHandler()
{
    delete QnPtzManageDialog::instance();
}

void QnWorkbenchPtzHandler::at_ptzSavePresetAction_triggered()
{
    auto widget = menu()->currentParameters(sender()).widget<QnMediaResourceWidget>();
    if (!widget || !widget->ptzController())
        return;
    QnResourcePtr resource = widget->resource()->toResourcePtr();

    // TODO: #sivanov Fix the text.
    if (resource->getStatus() == nx::vms::api::ResourceStatus::offline
        || resource->getStatus() == nx::vms::api::ResourceStatus::unauthorized)
    {
        messages::Ptz::failedToGetPosition(mainWindowWidget(),
        QnResourceDisplayInfo(resource).toString(
            appContext()->localSettings()->resourceInfoLevel()));
        return;
    }

    QScopedPointer<QnPtzPresetDialog> dialog(new QnPtzPresetDialog(mainWindowWidget()));
    dialog->setController(widget->ptzController());
    QScopedPointer<QnSingleCameraPtzHotkeysDelegate> hotkeysDelegate(
        new QnSingleCameraPtzHotkeysDelegate(dialog.data(), resource, context()));
    dialog->setHotkeysDelegate(hotkeysDelegate.data());
    dialog->exec();
}

void QnWorkbenchPtzHandler::at_ptzActivatePresetAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    auto widget = parameters.widget<QnMediaResourceWidget>();
    QString id = parameters.argument<QString>(Qn::PtzObjectIdRole);

    if (!widget || !widget->ptzController() || id.isEmpty())
        return;
    QnResourcePtr resource = widget->resource()->toResourcePtr();

    if (!widget->ptzController()->hasCapabilities(Ptz::PresetsPtzCapability))
    {
        // TODO: #sivanov Show appropriate error message?
        return;
    }

    if (widget->dewarpingParams().enabled)
    {
        auto params = widget->item()->dewarpingParams();
        params.enabled = true;
        widget->item()->setDewarpingParams(params);
    }

    if (widget->ptzController()->activatePreset(id, 1.0))
    {
        if (!widget->dewarpingParams().enabled) //< Do not jump to live if this is a fisheye camera.
            menu()->trigger(action::JumpToLiveAction, widget);
    }
    else
    {
        d->showSetPositionWarning(resource);
    }
}

void QnWorkbenchPtzHandler::at_ptzActivateTourAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    auto widget = parameters.widget<QnMediaResourceWidget>();
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

        const auto tour = std::find_if(tours.cbegin(), tours.cend(),
            [&id](const QnPtzTour &tour) { return id == tour.id; });
        if (tour == tours.cend())
            return;

        if (!tour->isValid(presets))
            return;
    }

    if (widget->dewarpingParams().enabled)
    {
        auto params = widget->item()->dewarpingParams();
        params.enabled = true;
        widget->item()->setDewarpingParams(params);
    }

    if (widget->ptzController()->activateTour(id))
    {
        if (!widget->dewarpingParams().enabled) //< Do not jump to live if this is a fisheye camera.
            menu()->trigger(action::JumpToLiveAction, widget);
    }
    else
    {
        d->showSetPositionWarning(resource);
    }
}

void QnWorkbenchPtzHandler::at_ptzActivateObjectAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    auto widget = parameters.widget<QnMediaResourceWidget>();
    if (!widget || !widget->ptzController())
        return;

    QString id = parameters.argument<QString>(Qn::PtzObjectIdRole).trimmed();
    if (id.isEmpty())
        return;

    QnPtzPresetList presets;
    if (!widget->ptzController()->getPresets(&presets))
        return;

    const auto preset = std::find_if(presets.cbegin(), presets.cend(),
        [&](const QnPtzPreset &preset) { return preset.id == id; });
    if (preset == presets.cend())
        menu()->trigger(action::PtzActivateTourAction, parameters);
    else
        menu()->trigger(action::PtzActivatePresetAction, parameters);
}


void QnWorkbenchPtzHandler::at_ptzManageAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    auto widget = parameters.widget<QnMediaResourceWidget>();

    if (!widget || !widget->ptzController())
        return;

    QnPtzManageDialog* dialog = QnPtzManageDialog::instance();
    if (!dialog)
        dialog = new QnPtzManageDialog(mainWindowWidget());

    if (dialog->isVisible() && !dialog->tryClose(false))
        return;

    auto res = widget->resource()->toResourcePtr();
    auto hotkeysDelegate =
        new QnSingleCameraPtzHotkeysDelegate(dialog, res, context());

    dialog->setWidget(widget);
    dialog->setHotkeysDelegate(hotkeysDelegate);
    dialog->show();
}

void QnWorkbenchPtzHandler::at_debugCalibratePtzAction_triggered()
{
    auto widget = menu()->currentParameters(sender()).widget<QnMediaResourceWidget>();
    if (!widget)
        return;
    QPointer<const QnMediaResourceWidget> guard(widget);
    QPointer<QnWorkbenchItem> itemGuard(widget->item());

    QnPtzControllerPtr controller = widget->ptzController();

    Vector position;
    if (!getDevicePosition(controller, &position))
        return;

    qreal startZ = 0.0;
    qreal endZ = 0.8;

    for (int i = 0; i <= 20; i++)
    {
        position.zoom = startZ + (endZ - startZ) * i / 20.0;
        controller->absoluteMove(CoordinateSpace::device, position, 1.0);

        QEventLoop loop;
        QTimer::singleShot(10000, &loop, SLOT(quit()));
        loop.exec();

        if (!guard || !itemGuard)
            break;

        Vector cameraPosition;
        getDevicePosition(controller, &cameraPosition);
        qDebug() << "SENT POSITION" << position << "GOT POSITION" << cameraPosition;

        if (!guard || !itemGuard)
            break;

        menu()->trigger(action::TakeScreenshotAction, action::Parameters(widget)
            .withArgument(
                Qn::FileNameRole,
                lit("PTZ_CALIBRATION_%1.jpg").arg(position.zoom, 0, 'f', 4)));
    }
}

void QnWorkbenchPtzHandler::at_debugGetPtzPositionAction_triggered()
{
    auto widget = menu()->currentParameters(sender()).widget<QnMediaResourceWidget>();
    if (!widget)
        return;
    QnPtzControllerPtr controller = widget->ptzController();

    Vector position;
    if (!getDevicePosition(controller, &position))
    {
        qDebug() << "COULD NOT GET POSITION";
    }
    else
    {
        qDebug() << "GOT POSITION" << position;
    }
}

void QnWorkbenchPtzHandler::at_ptzContinuousMoveAction_triggered()
{
    auto widget = qobject_cast<QnMediaResourceWidget*>(display()->widget(Qn::CentralRole));
    if (!widget)
        return;

    const auto parameters = menu()->currentParameters(sender());

    const auto controller = widget->ptzController();
    const auto item = widget->item();
    NX_ASSERT(item && controller);
    if (!item || !controller)
        return;

    auto speed = parameters.argument<QVector3D>(Qn::ItemDataRole::PtzSpeedRole);

    const bool ptzForced = parameters.argument(Qn::ItemDataRole::ForceRole).toBool();
    if (ptzForced)
        d->updatePtzState(!speed.isNull(), widget);

    if (((!qFuzzyIsNull(speed.x()) || !qFuzzyIsNull(speed.y())) && !d->checkPtzMoveEnabled()))
        return;

    if (!qFuzzyIsNull(speed.z()) && !d->checkPtzZoomEnabled())
        return;

    const auto rotation = item->rotation()
        + (item->data<bool>(Qn::ItemFlipRole, false) ? 0.0 : 180.0);
    speed = d->applyRotation(speed, rotation);

    Vector speedVector(speed.x(), speed.y(), 0.0, speed.z());
    controller->continuousMove(speedVector);

    if (speedVector.isNull())
        widget->clearActionText(2s);
    else if (!qFuzzyIsNull(speed.z()))
        widget->setActionText(speed.z() > 0.0 ? tr("Zooming in...") : tr("Zooming out..."));
    else
        widget->setActionText(tr("Moving..."));
}

void QnWorkbenchPtzHandler::at_ptzActivatePresetByIndexAction_triggered()
{
    if (!d->checkPtzEnabled())
        return;

    auto widget = qobject_cast<QnMediaResourceWidget*>(display()->widget(Qn::CentralRole));
    if (!widget)
        return;

    const auto parameters = menu()->currentParameters(sender());

    const auto controller = widget->ptzController();
    NX_ASSERT(controller);
    if (!controller)
        return;

    auto presetIndex = parameters.argument(Qn::ItemDataRole::PtzPresetIndexRole).toInt();

    QnPtzPresetList presetList;
    controller->getPresets(&presetList);
    std::sort(
        presetList.begin(),
        presetList.end(),
        [](QnPtzPreset f, QnPtzPreset s){ return f.name < s.name; });

    if (presetIndex < presetList.size() && presetIndex >= 0)
    {
        menu()->trigger(
            action::PtzActivateObjectAction,
            action::Parameters(widget)
                .withArgument(Qn::PtzObjectIdRole, presetList[presetIndex].id));
    }
}

void QnWorkbenchPtzHandler::at_ptzActivateObjectByHotkeyAction_triggered()
{
    if (!d->checkPtzEnabled())
        return;

    const auto widget = dynamic_cast<QnMediaResourceWidget*>(display()->widget(Qn::CentralRole));
    if (!widget)
        return;

    int hotkey = menu()->currentParameters(sender())
        .argument(Qn::ItemDataRole::PtzPresetIndexRole).toInt();

    nx::vms::client::core::ptz::HotkeysResourcePropertyAdaptor adaptor;
    adaptor.setResource(widget->resource()->toResourcePtr());

    QString objectId = adaptor.value().value(hotkey);
    if (objectId.isEmpty())
        return;

    menu()->trigger(
        action::PtzActivateObjectAction,
        action::Parameters(widget).withArgument(Qn::PtzObjectIdRole, objectId));
}

void QnWorkbenchPtzHandler::at_ptzFocusInAction_triggered()
{
    ptzFocusMove(/*speed*/ 1.0);
}

void QnWorkbenchPtzHandler::at_ptzFocusOutAction_triggered()
{
    ptzFocusMove(/*speed*/ -1.0);
}

void QnWorkbenchPtzHandler::ptzFocusMove(double speed)
{
    if (!d->checkPtzFocusEnabled())
        return;

    const auto widget = dynamic_cast<QnMediaResourceWidget*>(display()->widget(Qn::CentralRole));
    if (!widget)
        return;

    const auto parameters = menu()->currentParameters(sender());
    const bool ptzForced = parameters.argument(Qn::ItemDataRole::ForceRole).toBool();
    if (ptzForced)
        d->updatePtzState(speed != 0, widget);

    const auto controller = widget->ptzController();
    if (!NX_ASSERT(controller))
        return;

    //if (!controller->hasCapabilities(Ptz::VirtualPtzCapability))
    //    return;

    controller->continuousFocus(speed);

    // TODO: PtzInstrument has more detailed state description (e.g. it could switch to "Moving..."
    // right after the focusing is done). We need to unify messages or refactor the whole thing.
    if (qFuzzyIsNull(speed))
        widget->clearActionText(2s);
    else
        widget->setActionText(tr("Focusing..."));
}

void QnWorkbenchPtzHandler::at_ptzFocusAutoAction_triggered()
{
    if (!d->checkPtzFocusEnabled())
        return;

    const auto widget = dynamic_cast<QnMediaResourceWidget*>(display()->widget(Qn::CentralRole));
    if (!widget)
        return;

    const auto controller = widget->ptzController();
    if (!NX_ASSERT(controller))
        return;

    //if (!controller->hasCapabilities(Ptz::VirtualPtzCapability))
    //    return;

    /* Set auto focus. */
    controller->runAuxiliaryCommand(Ptz::ManualAutoFocusPtzTrait, QString());

    /* Update text. */
    widget->setActionText(tr("Focusing..."));
    widget->clearActionText(2s);
}
