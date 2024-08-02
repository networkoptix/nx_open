// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vms_rules_action_handler.h"

#include <memory.h>

#include <QtCore/QFileSystemWatcher>
#include <QtQml/QQmlEngine>

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/api/data/lookup_list_data.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/debug_utils/utils/debug_custom_actions.h>
#include <nx/vms/client/desktop/event_search/dialogs/event_log_dialog.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <ui/dialogs/common/non_modal_dialog_constructor.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/delayed.h>

#include "vms_rules_dialog.h"

namespace nx::vms::client::desktop::rules {

struct VmsRulesActionHandler::Private
{
    VmsRulesActionHandler* const q;
    std::unique_ptr<VmsRulesDialog> rulesDialog;
    QPointer<EventLogDialog> eventLogDialog;
    std::optional<rest::Handle> requestId;

    void cancelRequest()
    {
        if (requestId)
            q->connectedServerApi()->cancelRequest(*requestId);

        requestId = {};
    }

    void handleError(const QString& text)
    {
        if (rulesDialog)
            rulesDialog->setError(text);
    }

    void setFilter(const QnVirtualCameraResourceList& cameras)
    {
        if (!NX_ASSERT(cameras.size() <= 1, "Filter is not applicable for several cameras"))
            return;

        rulesDialog->setFilter(cameras.isEmpty() ? "" : cameras.front()->getId().toString());
    }

    void initialiseLookupLists()
    {
        if (q->lookupListManager()->isInitialized())
            return;

        if (requestId)
            q->connectedServerApi()->cancelRequest(*requestId);

        auto callback = nx::utils::guarded(
            q,
            [this](
                bool /*success*/,
                rest::Handle requestId,
                const rest::ErrorOrData<api::LookupListDataList>& result)
            {
                if (this->requestId != requestId)
                    return;

                this->requestId = std::nullopt;

                if (std::holds_alternative<network::rest::Result>(result))
                {
                    auto restResult = std::get<network::rest::Result>(result);
                    NX_WARNING(
                        this,
                        "Lookup Lists request %1 failed: %2",
                        requestId, restResult.errorString);

                    handleError(tr("Lookup lists network request failed"));
                    return;
                }

                auto lookupLists = std::get<api::LookupListDataList>(result);
                NX_VERBOSE(this, "Received %1 Lookup Lists entries", lookupLists.size());

                q->lookupListManager()->initialize(std::move(lookupLists));
            });

        requestId = q->connectedServerApi()->getLookupLists(std::move(callback), q->thread());
        NX_VERBOSE(this, "Send get lookup lists request (%1)", *requestId);
    }
};

VmsRulesActionHandler::VmsRulesActionHandler(QObject* parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    d(new Private{.q=this})
{
    registerDebugAction(
        "Vms Rules Dialog",
        [this](QnWorkbenchContext* context)
        {
            appContext()->qmlEngine()->clearComponentCache();
            auto dialog = std::make_unique<rules::VmsRulesDialog>(context->mainWindowWidget());

            QFileSystemWatcher watcher;
            watcher.addPath(
                appContext()->qmlEngine()->baseUrl().toLocalFile() + "/Nx/VmsRules/VmsRulesDialog.qml");

            connect(
                &watcher,
                &QFileSystemWatcher::fileChanged,
                this,
                [&dialog, this]
                {
                    appContext()->qmlEngine()->clearComponentCache();
                    dialog->reject();

                    executeDelayedParented(
                        [context = this->context()]{ debugActions()["Vms Rules Dialog"](context); },
                        this);
                });

            dialog->exec(Qt::NonModal);
        });

    connect(action(menu::OpenVmsRulesDialogAction), &QAction::triggered,
        this, &VmsRulesActionHandler::openVmsRulesDialog);

    connect(action(menu::CameraVmsRulesAction), &QAction::triggered,
        this, &VmsRulesActionHandler::openVmsRulesDialog);

    connect(action(menu::OpenEventLogAction), &QAction::triggered,
        this, &VmsRulesActionHandler::openEventLogDialog);
}

VmsRulesActionHandler::~VmsRulesActionHandler()
{
    d->cancelRequest();
}

void VmsRulesActionHandler::openVmsRulesDialog()
{
    if (!d->rulesDialog)
    {
        d->rulesDialog = std::make_unique<VmsRulesDialog>(mainWindowWidget());
        d->initialiseLookupLists();
    }

    const auto parameters = menu()->currentParameters(sender());
    d->setFilter(parameters.resources().filtered<QnVirtualCameraResource>());

    d->rulesDialog->open();
    d->rulesDialog->raise();
}

void VmsRulesActionHandler::openEventLogDialog()
{
    QnNonModalDialogConstructor<EventLogDialog> dialogConstructor(
        d->eventLogDialog,
        [this] { return new EventLogDialog(mainWindowWidget()); });

    const auto parameters = menu()->currentParameters(sender());
    UuidSet eventDeviceIds;
    for (const auto& device: parameters.resources().filtered<QnVirtualCameraResource>())
        eventDeviceIds.insert(device->getId());

    d->eventLogDialog->disableUpdateData();

    if (parameters.hasArgument(Qn::TimePeriodRole))
    {
        const auto period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);
        d->eventLogDialog->setDateRange(period.startTimeMs, period.endTimeMs());
    }
    else
    {
        d->eventLogDialog->resetDateRange();
    }
    d->eventLogDialog->setEventType(parameters.argument(Qn::EventTypeRole, QString()));
    d->eventLogDialog->setActionType(parameters.argument(Qn::ActionTypeRole, QString()));
    d->eventLogDialog->setEventDevices(eventDeviceIds);

    d->eventLogDialog->enableUpdateData();
}

} // namespace nx::vms::client::desktop::rules
