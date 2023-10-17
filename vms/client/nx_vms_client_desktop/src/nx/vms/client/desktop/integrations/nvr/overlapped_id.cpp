// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "overlapped_id.h"

#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/menu/action_conditions.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/menu_factory.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>

#include "overlapped_id_dialog.h"
#include "overlapped_id_loader.h"
#include "overlapped_id_state.h"
#include "overlapped_id_store.h"

using namespace nx::vms::client::desktop::menu;

namespace nx::vms::client::desktop::integrations {

class IsNvrNodeCondition: public Condition
{
public:
    virtual ActionVisibility check(const Parameters& parameters, WindowContext* context) override;
};

ActionVisibility IsNvrNodeCondition::check(
    const Parameters& parameters,
    WindowContext* context)
{
    bool isNvr = false;
    QString groupId;

    const auto& resources = parameters.resources();
    for (const auto& resource: resources)
    {
        const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
        if (camera->isNvr())
            isNvr = true;

        const auto cameraGroupId = camera->getGroupId();

        // The checks hide action if user selected several different NVR nodes.
        if (groupId.isEmpty())
        {
            if (cameraGroupId.isEmpty())
                return InvisibleAction;

            groupId = cameraGroupId;
        }
        else if (groupId != cameraGroupId)
        {
            return InvisibleAction;
        }
    }

    return isNvr ? EnabledAction : InvisibleAction;
}

OverlappedIdIntegration::OverlappedIdIntegration(QObject* parent):
    base_type(parent),
    m_store(new OverlappedIdStore())
{
}

OverlappedIdIntegration::~OverlappedIdIntegration()
{
}

void OverlappedIdIntegration::registerActions(menu::MenuFactory* factory)
{
    using namespace menu;

    const auto action = factory->registerAction()
        .mode(DesktopMode)
        .flags(Tree | MultiTarget | ResourceTarget)
        .text(tr("Overlapped ID..."))
        .requiredTargetPermissions(Qn::Permission::ViewFootagePermission)
        .condition(condition::hasFlags(Qn::live_cam, /*exclude*/ Qn::removed, MatchMode::any)
            && condition::treeNodeType(ResourceTree::NodeType::recorder)
            && ConditionWrapper(new IsNvrNodeCondition())
        )
        .action();

    connect(action, &QAction::triggered, this,
        [this, context = action->windowContext()] { openOverlappedIdDialog(context); });
}

void OverlappedIdIntegration::openOverlappedIdDialog(WindowContext* context)
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
            auto connection = context->system()->connectedServerApi();
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

                    context->workbenchContext()->navigator()->reopenPlaybackConnection(cameras);
                    context->workbenchContext()->navigator()->setPlaying(true);
                });

            context->workbenchContext()->navigator()->setPlaying(false);
            connection->setOverlappedId(
                groupId,
                m_store->state().currentId,
                callback,
                thread());
        });

    OverlappedIdLoader loader(context->system(), groupId, m_store, this);
    loader.start();

    dialog->exec();
}

} // namespace nx::vms::client::desktop::integrations
