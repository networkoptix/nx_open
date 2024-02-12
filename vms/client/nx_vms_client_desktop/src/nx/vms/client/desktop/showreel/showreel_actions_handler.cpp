// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "showreel_actions_handler.h"

#include <QtGui/QAction>

#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/string.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/messages/resources_messages.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/showreel/showreel_manager.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_showreel_manager.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_state_manager.h>

#include "showreel_executor.h"
#include "showreel_review_controller.h"
#include "showreel_state_manager.h"

namespace nx::vms::client::desktop {

namespace action = ui::action;

ShowreelActionsHandler::ShowreelActionsHandler(QObject* parent):
    base_type(parent),
    QnSessionAwareDelegate(parent),
    m_executor(new ShowreelExecutor(this)),
    m_reviewController(new ShowreelReviewController(this))
{
    connect(systemContext()->showreelManager(),
        &common::ShowreelManager::showreelChanged,
        m_executor,
        &ShowreelExecutor::updateShowreel);

    connect(systemContext()->showreelManager(),
        &common::ShowreelManager::showreelRemoved,
        m_executor,
        &ShowreelExecutor::stopShowreel);

    connect(action(action::NewShowreelAction), &QAction::triggered, this,
        [this]()
        {
            NX_ASSERT(context()->user());
            if (!context()->user())
                return;

            QStringList usedNames;
            for (const auto& showreel: systemContext()->showreelManager()->showreels())
                usedNames << showreel.name;

            nx::vms::api::ShowreelData showreel;
            showreel.id = nx::Uuid::createUuid();
            showreel.parentId = context()->user()->getId();
            showreel.name = nx::utils::generateUniqueString(
                usedNames, tr("Showreel"), tr("Showreel %1"));
            systemContext()->showreelManager()->addOrUpdateShowreel(showreel);
            saveShowreelToServer(showreel);
            menu()->trigger(action::SelectNewItemAction, {Qn::UuidRole, showreel.id});
            menu()->trigger(action::ReviewShowreelAction, {Qn::UuidRole, showreel.id});
        });

    connect(action(action::MakeShowreelAction), &QAction::triggered, this,
        [this]()
        {
            if (!menu()->triggerIfPossible(action::NewShowreelAction))
                return;
            const auto parameters = menu()->currentParameters(sender());
            menu()->trigger(action::DropResourcesAction, parameters);
            menu()->trigger(action::SaveCurrentShowreelAction);
        });

    connect(action(action::RenameShowreelAction), &QAction::triggered, this,
        [this]()
        {
            const auto parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<nx::Uuid>(Qn::UuidRole);
            auto showreel = systemContext()->showreelManager()->showreel(id);
            if (!showreel.isValid())
                return;

            const auto userId = context()->user()->getId();

            const QString name = parameters.argument<QString>(Qn::ResourceNameRole).trimmed();

            // Ask to override showreel with the same name (if any). Create local copy to avoid crash.
            const auto showreels = systemContext()->showreelManager()->showreels();
            for (const auto& other: showreels)
            {
                if (other.id == id)
                    continue;

                if (other.parentId != userId)
                    continue;

                if (other.name == name)
                {
                    if (!ui::messages::Resources::overrideShowreel(mainWindowWidget()))
                        return;

                    systemContext()->showreelManager()->removeShowreel(other.id);
                    removeShowreelFromServer(other.id);
                }
            }

            showreel.name = name;
            systemContext()->showreelManager()->addOrUpdateShowreel(showreel);
            saveShowreelToServer(showreel);
        });

    connect(action(action::RemoveShowreelAction), &QAction::triggered, this,
        [this]()
        {
            const auto parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<nx::Uuid>(Qn::UuidRole);
            NX_ASSERT(!id.isNull());

            const auto showreel = systemContext()->showreelManager()->showreel(id);
            if (!showreel.name.isEmpty())
            {
                QnSessionAwareMessageBox messageBox(mainWindowWidget());
                messageBox.setIcon(QnMessageBoxIcon::Question);
                messageBox.setText(tr("Delete Showreel %1?").arg(showreel.name));
                messageBox.setStandardButtons(QDialogButtonBox::Cancel);
                messageBox.addCustomButton(QnMessageBoxCustomButton::Delete,
                    QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);
                if (messageBox.exec() == QDialogButtonBox::Cancel)
                    return;
            }

            systemContext()->showreelManager()->removeShowreel(id);
            removeShowreelFromServer(id);
        });

    connect(action(action::ToggleShowreelModeAction), &QAction::toggled, this,
        [this](bool toggled)
        {
            const auto parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<nx::Uuid>(Qn::UuidRole);

            if (!toggled)
            {
                NX_ASSERT(id.isNull());
                m_executor->stopCurrentShowreel();
            }

            if (toggled && id.isNull())
            {
                const auto reviewShowreelId = workbench()->currentLayout()->data(
                    Qn::ShowreelUuidRole).value<nx::Uuid>();

                // Start Showreel on a review layout must start the layout showreel.
                if (!reviewShowreelId.isNull())
                    id = reviewShowreelId;
            }

            if (id.isNull())
            {
                if (toggled)
                    m_executor->startSingleLayoutShowreel();
            }
            else
            {
                NX_ASSERT(toggled);
                m_executor->startShowreel(systemContext()->showreelManager()->showreel(id));
                //TODO: #spanasenko Is it necessary here?
                //context()->instance<QnWorkbenchStateManager>()->saveState();
            }
            context()->action(action::FullscreenAction)->setChecked(true);
        });

    connect(action(action::PreviousLayoutAction), &QAction::triggered, this,
        [this]
        {
            if (action(action::ToggleShowreelModeAction)->isChecked())
                m_executor->prevShowreelStep();
        });

    connect(action(action::NextLayoutAction), &QAction::triggered, this,
        [this]
        {
            if (action(action::ToggleShowreelModeAction)->isChecked())
                m_executor->nextShowreelStep();
        });

    connect(action(action::SaveShowreelAction), &QAction::triggered, this,
        [this]
        {
            const auto parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<nx::Uuid>(Qn::UuidRole);
            auto showreel = systemContext()->showreelManager()->showreel(id);
            if (!showreel.isValid())
                return;
            saveShowreelToServer(showreel);
        });

    connect(action(action::EscapeHotkeyAction), &QAction::triggered, this,
        [this]
        {
            if (action(action::ToggleShowreelModeAction)->isChecked())
                action(action::ToggleShowreelModeAction)->toggle();

            // Just for safety
            NX_ASSERT(m_executor->runningShowreel().isNull());
            m_executor->stopCurrentShowreel();
        });

    connect(action(action::SuspendCurrentShowreelAction), &QAction::triggered, this,
        [this]
        {
            m_executor->suspendCurrentShowreel();
        });

    connect(action(action::ResumeCurrentShowreelAction), &QAction::triggered, this,
        [this]
        {
            m_executor->resumeCurrentShowreel();
        });
}

ShowreelActionsHandler::~ShowreelActionsHandler()
{
}

bool ShowreelActionsHandler::tryClose(bool /*force*/)
{
    m_executor->stopCurrentShowreel();
    return true;
}

void ShowreelActionsHandler::forcedUpdate()
{
    // Do nothing
}

nx::Uuid ShowreelActionsHandler::runningShowreel() const
{
    return m_executor->runningShowreel();
}

void ShowreelActionsHandler::saveShowreelToServer(const nx::vms::api::ShowreelData& showreel)
{
    if (!NX_ASSERT(systemContext()->showreelManager()->showreel(showreel.id).isValid()))
        return;

    auto stateManager = systemContext()->showreelStateManager();

    if (const auto connection = systemContext()->messageBusConnection())
    {
        int reqId = connection->getShowreelManager(Qn::kSystemAccess)->save(
            showreel,
            [showreel, stateManager](int reqId, ec2::ErrorCode errorCode)
            {
                stateManager->removeSaveRequest(showreel.id, reqId);
                if (stateManager->hasSaveRequests(showreel.id))
                    return;

                stateManager->markBeingSaved(showreel.id, false);
                if (errorCode == ec2::ErrorCode::ok)
                    stateManager->markChanged(showreel.id, false);
            },
            this);
        const bool success = (reqId > 0);
        if (success)
        {
            stateManager->markBeingSaved(showreel.id, true);
            stateManager->addSaveRequest(showreel.id, reqId);
        }
    };
}

void ShowreelActionsHandler::removeShowreelFromServer(const nx::Uuid& showreelId)
{
    if (const auto connection = systemContext()->messageBusConnection())
    {
        connection->getShowreelManager(Qn::kSystemAccess)->remove(
            showreelId, [](int /*reqId*/, ec2::ErrorCode) {});
    }
}

} // namespace nx::vms::client::desktop
