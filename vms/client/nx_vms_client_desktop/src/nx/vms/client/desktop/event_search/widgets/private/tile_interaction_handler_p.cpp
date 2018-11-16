#include "tile_interaction_handler_p.h"

#include <QtCore/QModelIndex>
#include <QtWidgets/QApplication>

#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/camera_resource.h>
#include <ui/graphics/items/generic/graphics_message_box.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_navigator.h>
#include <utils/common/delayed.h>

#include <nx/utils/log/assert.h>
#include <nx/utils/pending_operation.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/event_search/widgets/event_ribbon.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;
using namespace ui::action;

namespace {

milliseconds doubleClickInterval()
{
    return milliseconds(QApplication::doubleClickInterval());
}

} // namespace

TileInteractionHandler::~TileInteractionHandler()
{
}

TileInteractionHandler* TileInteractionHandler::install(EventRibbon* ribbon)
{
    NX_ASSERT(ribbon);
    if (!ribbon)
        return nullptr;

    auto previous = ribbon->findChild<TileInteractionHandler*>();
    return previous ? previous : new TileInteractionHandler(ribbon);
}

TileInteractionHandler::TileInteractionHandler(EventRibbon* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_ribbon(parent),
    m_showPendingMessages(new nx::utils::PendingOperation())
{
    NX_ASSERT(m_ribbon);
    if (!m_ribbon)
        return;

    m_showPendingMessages->setFlags(nx::utils::PendingOperation::FireOnlyWhenIdle);
    m_showPendingMessages->setCallback(
        [this]()
        {
            for (const auto& message: m_pendingMessages)
                showMessage(message);

            m_pendingMessages.clear();
        });

    connect(m_ribbon.data(), &EventRibbon::linkActivated, this,
        [this](const QModelIndex& index, const QString& link)
        {
            m_ribbon->model()->setData(index, link, Qn::ActivateLinkRole);
        });

    connect(m_ribbon.data(), &EventRibbon::clicked, this,
        [this](const QModelIndex& index, Qt::MouseButton button, Qt::KeyboardModifiers modifiers)
        {
            if (button == Qt::LeftButton && !modifiers.testFlag(Qt::ControlModifier))
            {
                if (!m_ribbon->model()->setData(index, QVariant(), Qn::DefaultNotificationRole))
                    navigateToSource(index);
            }
            else if (button == Qt::LeftButton && modifiers.testFlag(Qt::ControlModifier)
                || button == Qt::MiddleButton)
            {
                openSource(index, true /*inNewTab*/);
            }
        });

    connect(m_ribbon.data(), &EventRibbon::doubleClicked, this,
        [this](const QModelIndex& index)
        {
            openSource(index, false /*inNewTab*/);
        });

    connect(m_ribbon.data(), &EventRibbon::dragStarted, this,
        [this](const QModelIndex& index)
        {
            //TODO: #vkutin Implement me!
        });
}

void TileInteractionHandler::navigateToSource(const QModelIndex& index)
{
    // Obtain requested time.
    const auto timestamp = index.data(Qn::TimestampRole);
    if (!timestamp.canConvert<microseconds>())
        return;

    // Obtain requested camera list.
    const auto cameraList = index.data(Qn::ResourceListRole).value<QnResourceList>()
        .filtered<QnVirtualCameraResource>();

    QnVirtualCameraResourceList openCameras;

    // Find which cameras are opened on current layout.
    if (auto layout = workbench()->currentLayout())
    {
        openCameras = cameraList.filtered(
            [layout](const QnVirtualCameraResourcePtr& camera)
            {
                return !layout->items(camera).empty();
            });
    }

    // If not all cameras are opened, show proper message after double click delay.
    if (openCameras.size() != cameraList.size())
    {
        const auto message = tr("Double click to add camera(s) to current layout "
            "or ctrl+click to open in a new tab.", "", cameraList.size());

        showMessageDelayed(message, doubleClickInterval());
    }

    // Single-select first of opened requested cameras.
    const auto camera = openCameras.empty() ? QnVirtualCameraResourcePtr() : openCameras.front();
    if (camera)
    {
        menu()->triggerIfPossible(GoToLayoutItemAction, Parameters(camera)
            .withArgument(Qn::ForceRole, ini().raiseCameraFromClickedTile));
    }

    // If timeline is hidden, do no navigation.
    if (!navigator()->isTimelineRelevant())
        return;

    // If requested time is outside timeline range, show proper message after double click delay.
    const auto timelineRange = navigator()->timelineRange();
    auto navigationTime = timestamp.value<microseconds>();

    if (!timelineRange.contains(duration_cast<milliseconds>(navigationTime)))
    {
        showMessageDelayed(tr("No available archive."), doubleClickInterval());
        return;
    }

    // In case of requested time within the last minute, navigate to live instead.
    if (const bool lastMinute = navigationTime > (timelineRange.endTime() - 1min))
        navigationTime = microseconds(DATETIME_NOW);

    // Perform navigation.
    menu()->triggerIfPossible(JumpToTimeAction,
        Parameters().withArgument(Qn::TimestampRole, navigationTime));
}

void TileInteractionHandler::openSource(const QModelIndex& index, bool inNewTab)
{
    const auto cameraList = index.data(Qn::ResourceListRole).value<QnResourceList>()
        .filtered<QnVirtualCameraResource>();

    if (cameraList.empty())
        return;

    Parameters parameters(cameraList);

    const auto timestamp = index.data(Qn::TimestampRole);
    if (timestamp.canConvert<microseconds>())
    {
        parameters.setArgument(Qn::ItemTimeRole,
            duration_cast<milliseconds>(timestamp.value<microseconds>()).count());
    }

    nx::utils::ScopedConnection connection;
    if (!inNewTab && cameraList.size() == 1 && workbench()->currentLayout())
    {
        connection.reset(connect(workbench()->currentLayout(), &QnWorkbenchLayout::itemAdded, this,
            [this](const QnWorkbenchItem* item)
            {
                menu()->triggerIfPossible(GoToLayoutItemAction, Parameters()
                    .withArgument(Qn::ItemUuidRole, item->uuid())
                    .withArgument(Qn::ForceRole, ini().raiseCameraFromClickedTile));
            }));
    }

    hideMessages();

    const auto action = inNewTab ? OpenInNewTabAction : DropResourcesAction;
    menu()->triggerIfPossible(action, parameters);
}

void TileInteractionHandler::showMessage(const QString& text)
{
    const auto box = QnGraphicsMessageBox::information(text);
    m_messages.insert(box);

    connect(box, &QObject::destroyed, this,
        [this](QObject* sender) { m_messages.remove(static_cast<QnGraphicsMessageBox*>(sender)); });
}

void TileInteractionHandler::showMessageDelayed(const QString& text, milliseconds delay)
{
    m_pendingMessages.push_back(text);
    m_showPendingMessages->setInterval(delay);
    m_showPendingMessages->requestOperation();
}

void TileInteractionHandler::hideMessages()
{
    for (auto box: m_messages)
        box->hideAnimated();

    m_pendingMessages.clear();
}

} // namespace nx::vms::client::desktop
