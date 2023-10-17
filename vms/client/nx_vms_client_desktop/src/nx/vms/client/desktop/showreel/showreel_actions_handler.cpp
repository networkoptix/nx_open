// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "showreel_actions_handler.h"

#include <QtGui/QAction>

#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/string.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/system_context.h>
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

ShowreelActionsHandler::ShowreelActionsHandler(QObject* parent):
    base_type(parent),
    QnSessionAwareDelegate(parent),
    m_executor(new ShowreelExecutor(this)),
    m_reviewController(new ShowreelReviewController(this))
{
    connect(system()->showreelManager(),
        &common::ShowreelManager::showreelChanged,
        m_executor,
        &ShowreelExecutor::updateShowreel);

    connect(system()->showreelManager(),
        &common::ShowreelManager::showreelRemoved,
        m_executor,
        &ShowreelExecutor::stopShowreel);

    connect(action(menu::NewShowreelAction), &QAction::triggered, this,
        [this]()
        {
            if (!NX_ASSERT(system()->user()))
                return;

            QStringList usedNames;
            for (const auto& showreel: system()->showreelManager()->showreels())
                usedNames << showreel.name;

            nx::vms::api::ShowreelData showreel;
            showreel.id = QnUuid::createUuid();
            showreel.parentId = system()->user()->getId();
            showreel.name = nx::utils::generateUniqueString(
                usedNames, tr("Showreel"), tr("Showreel %1"));
            system()->showreelManager()->addOrUpdateShowreel(showreel);
            saveShowreelToServer(showreel);
            menu()->trigger(menu::SelectNewItemAction, {Qn::UuidRole, showreel.id});
            menu()->trigger(menu::ReviewShowreelAction, {Qn::UuidRole, showreel.id});
        });

    connect(action(menu::MakeShowreelAction), &QAction::triggered, this,
        [this]()
        {
            if (!menu()->triggerIfPossible(menu::NewShowreelAction))
                return;
            const auto parameters = menu()->currentParameters(sender());
            menu()->trigger(menu::DropResourcesAction, parameters);
            menu()->trigger(menu::SaveCurrentShowreelAction);
        });

    connect(action(menu::RenameShowreelAction), &QAction::triggered, this,
        [this]()
        {
            const auto parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<QnUuid>(Qn::UuidRole);
            auto showreel = system()->showreelManager()->showreel(id);
            if (!showreel.isValid())
                return;

            const auto userId = system()->user()->getId();

            const QString name = parameters.argument<QString>(Qn::ResourceNameRole).trimmed();

            // Ask to override showreel with the same name (if any). Create local copy to avoid crash.
            const auto showreels = system()->showreelManager()->showreels();
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

                    system()->showreelManager()->removeShowreel(other.id);
                    removeShowreelFromServer(other.id);
                }
            }

            showreel.name = name;
            system()->showreelManager()->addOrUpdateShowreel(showreel);
            saveShowreelToServer(showreel);
        });

    connect(action(menu::RemoveShowreelAction), &QAction::triggered, this,
        [this]()
        {
            const auto parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<QnUuid>(Qn::UuidRole);
            NX_ASSERT(!id.isNull());

            const auto showreel = system()->showreelManager()->showreel(id);
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

            system()->showreelManager()->removeShowreel(id);
            removeShowreelFromServer(id);
        });

    connect(action(menu::ToggleShowreelModeAction), &QAction::toggled, this,
        [this](bool toggled)
        {
            const auto parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<QnUuid>(Qn::UuidRole);

            if (!toggled)
            {
                NX_ASSERT(id.isNull());
                m_executor->stopCurrentShowreel();
            }

            if (toggled && id.isNull())
            {
                const auto reviewShowreelId = workbench()->currentLayout()->data(
                    Qn::ShowreelUuidRole).value<QnUuid>();

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
                m_executor->startShowreel(system()->showreelManager()->showreel(id));
                //TODO: #spanasenko Is it necessary here?
                //windowContext()->workbenchStateManager()->saveState();
            }
            action(menu::FullscreenAction)->setChecked(true);
        });

    connect(action(menu::PreviousLayoutAction), &QAction::triggered, this,
        [this]
        {
            if (action(menu::ToggleShowreelModeAction)->isChecked())
                m_executor->prevShowreelStep();
        });

    connect(action(menu::NextLayoutAction), &QAction::triggered, this,
        [this]
        {
            if (action(menu::ToggleShowreelModeAction)->isChecked())
                m_executor->nextShowreelStep();
        });

    connect(action(menu::SaveShowreelAction), &QAction::triggered, this,
        [this]
        {
            const auto parameters = menu()->currentParameters(sender());
            auto id = parameters.argument<QnUuid>(Qn::UuidRole);
            auto showreel = system()->showreelManager()->showreel(id);
            if (!showreel.isValid())
                return;
            saveShowreelToServer(showreel);
        });

    connect(action(menu::EscapeHotkeyAction), &QAction::triggered, this,
        [this]
        {
            if (action(menu::ToggleShowreelModeAction)->isChecked())
                action(menu::ToggleShowreelModeAction)->toggle();

            // Just for safety
            NX_ASSERT(m_executor->runningShowreel().isNull());
            m_executor->stopCurrentShowreel();
        });

    connect(action(menu::SuspendCurrentShowreelAction), &QAction::triggered, this,
        [this]
        {
            m_executor->suspendCurrentShowreel();
        });

    connect(action(menu::ResumeCurrentShowreelAction), &QAction::triggered, this,
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

QnUuid ShowreelActionsHandler::runningShowreel() const
{
    return m_executor->runningShowreel();
}

void ShowreelActionsHandler::saveShowreelToServer(const nx::vms::api::ShowreelData& showreel)
{
    if (!NX_ASSERT(system()->showreelManager()->showreel(showreel.id).isValid()))
        return;

    auto stateManager = system()->showreelStateManager();

    if (const auto connection = system()->messageBusConnection())
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

void ShowreelActionsHandler::removeShowreelFromServer(const QnUuid& showreelId)
{
    if (const auto connection = system()->messageBusConnection())
    {
        connection->getShowreelManager(Qn::kSystemAccess)->remove(
            showreelId, [](int /*reqId*/, ec2::ErrorCode) {});
    }
}

} // namespace nx::vms::client::desktop
