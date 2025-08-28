// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "vms_rules_action_handler.h"

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/api/data/lookup_list_data.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/debug_utils/utils/debug_custom_actions.h>
#include <nx/vms/client/desktop/event_search/dialogs/event_log_dialog.h>
#include <nx/vms/client/desktop/menu/action.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <ui/dialogs/common/non_modal_dialog_constructor.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/delayed.h>

#include "vms_rules_dialog.h"

namespace nx::vms::client::desktop::rules {

struct VmsRulesActionHandler::Private
{
    VmsRulesActionHandler* const q;
    QPointer<VmsRulesDialog> rulesDialog;
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
            emit rulesDialog->error(text);
    }

    void setFilter(const QnVirtualCameraResourceList& cameras)
    {
        if (!NX_ASSERT(cameras.size() <= 1, "Filter is not applicable for several cameras"))
            return;

        rulesDialog->setFilter(
            cameras.isEmpty() ? "" : cameras.front()->getId().toSimpleString());
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
                rest::ErrorOrData<api::LookupListDataList> lookupLists)
            {
                if (this->requestId != requestId)
                    return;

                this->requestId = std::nullopt;

                if (!lookupLists)
                {
                    auto restResult = lookupLists.error();
                    NX_WARNING(
                        this,
                        "Lookup Lists request %1 failed: %2",
                        requestId, restResult.errorString);

                    handleError(tr("Lookup lists network request failed"));
                    return;
                }

                NX_VERBOSE(this, "Received %1 Lookup Lists entries", lookupLists->size());

                q->lookupListManager()->initialize(std::move(*lookupLists));
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
    connect(action(menu::OpenVmsRulesDialogAction), &QAction::triggered,
        this, &VmsRulesActionHandler::openVmsRulesDialog);

    connect(action(menu::CameraVmsRulesAction), &QAction::triggered,
        this, &VmsRulesActionHandler::openVmsRulesDialog);

    connect(action(menu::OpenEventLogAction), &QAction::triggered,
        this, &VmsRulesActionHandler::openEventLogDialog);

    connect(
        windowContext(),
        &WindowContext::systemChanged,
        this,
        [this]
        {
            d->cancelRequest();

            if (d->rulesDialog)
                d->rulesDialog->deleteLater();

            if (d->eventLogDialog)
                d->eventLogDialog->deleteLater();
        });
}

VmsRulesActionHandler::~VmsRulesActionHandler()
{
    d->cancelRequest();
}

void VmsRulesActionHandler::openVmsRulesDialog()
{
    if (!d->rulesDialog)
    {
        d->rulesDialog = new VmsRulesDialog(mainWindowWidget());
        d->initialiseLookupLists();
    }

    const auto parameters = menu()->currentParameters(sender());
    d->setFilter(parameters.resources().filtered<QnVirtualCameraResource>());

    if (d->rulesDialog->window()->isVisible()
        && QGuiApplication::focusWindow() != d->rulesDialog->window())
    {
        d->rulesDialog->raise();
    }
    else
    {
        d->rulesDialog->open();
    }
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

    if (parameters.hasArgument(Qn::EventTypeRole))
        d->eventLogDialog->setEventType(parameters.argument<QString>(Qn::EventTypeRole));

    if (parameters.hasArgument(Qn::ActionTypeRole))
        d->eventLogDialog->setActionType(parameters.argument<QString>(Qn::ActionTypeRole));

    if (!eventDeviceIds.empty())
        d->eventLogDialog->setEventDevices(eventDeviceIds);

    d->eventLogDialog->enableUpdateData();
}

} // namespace nx::vms::client::desktop::rules
