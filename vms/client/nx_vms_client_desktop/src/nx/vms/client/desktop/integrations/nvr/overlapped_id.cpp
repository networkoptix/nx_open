// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "overlapped_id.h"

#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_conditions.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/menu_factory.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>

#include "overlapped_id_store.h"
#include "overlapped_id_state.h"
#include "overlapped_id_dialog.h"

namespace nx::vms::client::desktop::integrations {

OverlappedIdIntegration::OverlappedIdIntegration(QObject* parent):
    base_type(parent),
    m_store(new OverlappedIdStore())
{
}

OverlappedIdIntegration::~OverlappedIdIntegration()
{
}

void OverlappedIdIntegration::registerActions(ui::action::MenuFactory* factory)
{
    using namespace ui::action;

    const auto action = factory->registerAction()
        .mode(DesktopMode)
        .flags(Scene | Tree | SingleTarget | MultiTarget | ResourceTarget)
        .text(tr("Overlapped ID..."))
        .requiredGlobalPermission(GlobalPermission::viewArchive)
        .condition(condition::hasFlags(Qn::live_cam, /*exclude*/ Qn::removed, MatchMode::any)
            && condition::scoped(
                SceneScope,
                !condition::isLayoutTourReviewMode()
                && !condition::isPreviewSearchMode())
            && condition::treeNodeType({ResourceTree::NodeType::recorder})
            && ConditionWrapper(
                new ResourceCondition(
                    [this](const QnResourcePtr& resource)
                    {
                        const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
                        return camera->isNvr();
                    },
                    MatchMode::all))
        )
        .action();

    connect(action, &QAction::triggered, this,
        [this, context = action->context()] { openOverlappedIdDialog(context); });
}

void OverlappedIdIntegration::openOverlappedIdDialog(QnWorkbenchContext* context)
{
    const auto parameters = context->menu()->currentParameters(sender());
    auto cameras = parameters.resources().filtered<QnVirtualCameraResource>();
    if (!NX_ASSERT(!cameras.isEmpty(), "Invalid number of cameras."))
        return;

    m_store->reset();

    std::unique_ptr<OverlappedIdDialog> dialog(
        new OverlappedIdDialog(m_store.get(), context->mainWindowWidget()));

    const auto groupId = cameras.first()->getGroupId();

    connect(dialog.get(), &OverlappedIdDialog::accepted,
        [this, context, groupId, cameras]()
        {
            auto connection = connectedServerApi();
            if (!connection)
                return;

            auto callback = nx::utils::guarded(this,
                [this, context, cameras](
                    bool success,
                    rest::Handle /*requestId*/,
                    nx::vms::api::OverlappedIdResponse /*result*/)
                {
                    if (!success)
                        NX_WARNING(this, "Overlapped id is not set.");

                    context->navigator()->reopenPlaybackConnection(cameras);
                });

            connection->setOverlappedId(
                groupId,
                m_store->state().currentId,
                callback,
                thread());
        });

    auto callback = nx::utils::guarded(this,
        [this](
            bool success,
            rest::Handle /*requestId*/,
            nx::vms::api::OverlappedIdResponse result)
        {
            if (!success)
            {
                NX_WARNING(this, "Received invalid overlapped id data.");
                return;
            }

            m_store->setCurrentId(result.currentOverlappedId);
            m_store->setIdList(result.availableOverlappedIds);
        });

    auto connection = connectedServerApi();
    if (!connection)
        return;

    connection->getOverlappedIds(groupId, callback, thread());

    dialog->exec();
}

} // namespace nx::vms::client::desktop::integrations
