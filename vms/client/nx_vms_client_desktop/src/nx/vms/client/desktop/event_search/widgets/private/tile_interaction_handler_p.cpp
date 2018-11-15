#include "tile_interaction_handler_p.h"

#include <QtCore/QModelIndex>

#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/camera_resource.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/event_search/widgets/event_ribbon.h>

namespace nx::vms::client::desktop {

using namespace std::chrono;
using namespace ui::action;

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
    m_ribbon(parent)
{
    NX_ASSERT(m_ribbon);
    if (!m_ribbon)
        return;

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

void TileInteractionHandler::navigateToSource(const QModelIndex& index) const
{
    const auto timestamp = index.data(Qn::TimestampRole);
    if (!timestamp.isValid())
        return;

    const auto cameraList = index.data(Qn::ResourceListRole).value<QnResourceList>()
        .filtered<QnVirtualCameraResource>();

    const auto camera = cameraList.size() == 1
        ? cameraList.back()
        : QnVirtualCameraResourcePtr();

    if (camera)
    {
        menu()->triggerIfPossible(GoToLayoutItemAction, Parameters(camera)
            .withArgument(Qn::ForceRole, ini().raiseCameraFromClickedTile));
    }

    menu()->triggerIfPossible(JumpToTimeAction,
        Parameters().withArgument(Qn::TimestampRole, timestamp));
}

void TileInteractionHandler::openSource(const QModelIndex& index, bool inNewTab) const
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

    const auto action = inNewTab ? OpenInNewTabAction : DropResourcesAction;
    menu()->triggerIfPossible(action, parameters);
}

} // namespace nx::vms::client::desktop
