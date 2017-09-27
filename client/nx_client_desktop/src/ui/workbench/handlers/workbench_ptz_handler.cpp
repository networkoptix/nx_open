#include "workbench_ptz_handler.h"

#include <QtCore/QEventLoop>
#include <QtCore/QTimer>

#include <QtWidgets/QAction>

#include <api/app_server_connection.h>

#include <utils/resource_property_adaptors.h>

#include <common/common_globals.h>

#include <core/ptz/abstract_ptz_controller.h>
#include <core/ptz/ptz_preset.h>
#include <core/ptz/ptz_tour.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource_display_info.h>

#include <client/client_settings.h>

#include <nx/client/desktop/ui/messages/ptz_messages.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>
#include <ui/dialogs/ptz_preset_dialog.h>
#include <ui/dialogs/ptz_manage_dialog.h>

#include <ui/graphics/items/resource/media_resource_widget.h>

#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>

using namespace nx::client::desktop::ui;

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
        m_propertyHandler(new QnJsonResourcePropertyHandler<QnPtzIdByHotkeyHash>())
    {
    }

    ~QnSingleCameraPtzHotkeysDelegate() {}

    virtual QnPtzIdByHotkeyHash hotkeys() const override
    {
        QnPtzIdByHotkeyHash result;

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

    virtual void updateHotkeys(const QnPtzIdByHotkeyHash &value) override
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
    std::unique_ptr<QnJsonResourcePropertyHandler<QnPtzIdByHotkeyHash>> m_propertyHandler;
    mutable QnMutex m_mutex;
};


bool getDevicePosition(const QnPtzControllerPtr &controller, QVector3D *position)
{
    if (!controller->hasCapabilities(Ptz::AsynchronousPtzCapability))
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

    // TODO: #GDM #PTZ fix the text
    if (resource->getStatus() == Qn::Offline || resource->getStatus() == Qn::Unauthorized)
    {
        messages::Ptz::failedToGetPosition(mainWindow(),
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
    const auto parameters = menu()->currentParameters(sender());
    auto widget = parameters.widget<QnMediaResourceWidget>();
    QString id = parameters.argument<QString>(Qn::PtzObjectIdRole);

    if (!widget || !widget->ptzController() || id.isEmpty())
        return;
    QnResourcePtr resource = widget->resource()->toResourcePtr();

    if (!widget->ptzController()->hasCapabilities(Ptz::PresetsPtzCapability))
    {
        // TODO: #GDM #PTZ show appropriate error message?
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
            action(action::JumpToLiveAction)->trigger(); // TODO: #Elric ?
    }
    else
    {
        showSetPositionWarning(resource);
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
        QnItemDewarpingParams params = widget->item()->dewarpingParams();
        params.enabled = true;
        widget->item()->setDewarpingParams(params);
    }

    if (widget->ptzController()->activateTour(id))
    {
        if (!widget->dewarpingParams().enabled) // do not jump to live if this is a fisheye camera
            action(action::JumpToLiveAction)->trigger(); // TODO: #Elric ?
    }
    else
    {
        showSetPositionWarning(resource);
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
        dialog = new QnPtzManageDialog(mainWindow());

    if (dialog->isVisible() && !dialog->tryClose(false))
        return;

    auto res = widget->resource()->toResourcePtr();
    auto hotkeysDelegate =
        new QnSingleCameraPtzHotkeysDelegate(res, context());

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

        if (!guard || !itemGuard)
            break;

        QVector3D cameraPosition;
        getDevicePosition(controller, &cameraPosition);
        qDebug() << "SENT POSITION" << position << "GOT POSITION" << cameraPosition;

        if (!guard || !itemGuard)
            break;

        menu()->trigger(action::TakeScreenshotAction, action::Parameters(widget)
            .withArgument(Qn::FileNameRole, lit("PTZ_CALIBRATION_%1.jpg").arg(position.z(), 0, 'f', 4)));
    }
}

void QnWorkbenchPtzHandler::at_debugGetPtzPositionAction_triggered()
{
    auto widget = menu()->currentParameters(sender()).widget<QnMediaResourceWidget>();
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
        messages::Ptz::failedToSetPosition(mainWindow(),
            QnResourceDisplayInfo(resource).toString(qnSettings->extraInfoInTree()));
    }
    // TODO: #GDM #PTZ check other cases
}

void QnWorkbenchPtzHandler::at_ptzContinuousMoveAction_triggered()
{
    auto widget = qobject_cast<QnMediaResourceWidget*>(display()->widget(Qn::CentralRole));
    if (!widget)
        return;

    // Joystick-controlled action must occur only if ptz is active.
    const bool ptzActive = widget->options().testFlag(QnResourceWidget::ControlPtz);
    if (!ptzActive)
        return;

    const auto controller = widget->ptzController();
    const auto item = widget->item();
    NX_EXPECT(item && controller);
    if (!item || !controller)
        return;

    auto speed = menu()->currentParameters(sender())
        .argument<QVector3D>(Qn::ItemDataRole::PtzSpeedRole);

    const auto rotation = item->rotation()
        + (item->data<bool>(Qn::ItemFlipRole, false) ? 0.0 : 180.0);
    speed = applyRotation(speed, rotation);
    controller->continuousMove(speed);
}

void QnWorkbenchPtzHandler::at_ptzActivatePresetByIndexAction_triggered()
{
    auto widget = qobject_cast<QnMediaResourceWidget*>(display()->widget(Qn::CentralRole));
    if (!widget)
        return;

    // Joystick-controlled action must occur only if ptz is active.
    const bool ptzActive = widget->options().testFlag(QnResourceWidget::ControlPtz);
    if (!ptzActive)
        return;

    const auto controller = widget->ptzController();
    NX_EXPECT(controller);
    if (!controller)
        return;

    auto presetIndex = menu()->currentParameters(sender())
        .argument(Qn::ItemDataRole::PtzPresetIndexRole).toInt();

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

QVector3D QnWorkbenchPtzHandler::applyRotation(const QVector3D& speed, qreal rotation) const
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

