// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "delayed_data_loader.h"

#include <api/server_rest_connection.h>
#include <client/client_message_processor.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/lookup_list_data.h>
#include <nx/vms/api/rules/event_log.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/core/utils/log_strings_format.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/handlers/notification_action_executor.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <nx/vms/rules/engine.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

using nx::vms::client::core::AccessController;

struct DelayedDataLoader::Private
{
    DelayedDataLoader* const q;
    QSet<rest::Handle> requestIds;

    void cancelRequests()
    {
        if (auto serverApi = q->connectedServerApi())
        {
            for (const auto id: requestIds)
                serverApi->cancelRequest(id);
        }

        requestIds.clear();
    }

    void loadAcknowledge()
    {
        using namespace nx::vms::api::rules;

        auto callback = nx::utils::guarded(
            q,
            [this](
                bool success,
                rest::Handle requestId,
                rest::ErrorOrData<EventLogRecordList> records)
            {
                if (!requestIds.remove(requestId))
                    return;

                NX_LOG_RESPONSE(this, success, records,
                    "Acknowledge request %1 failed.", requestId);

                if (!success || !records)
                    return;

                NX_VERBOSE(this, "Received %1 acknowledge actions", records->size());

                appContext()->mainWindowContext()->workbenchContext()
                    ->instance<NotificationActionExecutor>()->setAcknowledge(std::move(*records));
            });

        if (auto serverApi = q->connectedServerApi())
        {
            auto requestId = serverApi->getEventsToAcknowledge(std::move(callback), q->thread());
            requestIds.insert(requestId);
            NX_VERBOSE(this, "Send get lookup lists request (%1)", requestId);
        }
    }

    void loadLookupLists()
    {
        using nx::vms::api::LookupListDataList;

        auto callback = nx::utils::guarded(
            q,
            [this](
                bool success,
                rest::Handle requestId,
                rest::ErrorOrData<LookupListDataList> lookupLists)
            {
                if (!requestIds.remove(requestId))
                    return;

                NX_LOG_RESPONSE(this, success, lookupLists,
                    "Lookup Lists request %1 failed.", requestId);

                if (!success || !lookupLists)
                    return;

                NX_VERBOSE(this, "Received %1 Lookup Lists entries", lookupLists->size());

                q->lookupListManager()->initialize(std::move(*lookupLists));
            });

        if (auto serverApi = q->connectedServerApi())
        {
            auto requestId = serverApi->getLookupLists(std::move(callback), q->thread());
            requestIds.insert(requestId);
            NX_VERBOSE(this, "Send get lookup lists request (%1)", requestId);
        }
    }

    void loadData()
    {
        // While there is only one piece of data, we can keep the architecture simple. Later we
        // will need to implement requests queue as in CloudCrossSystemContextDataLoader class.

        if (ini().lookupLists && q->accessController()->hasPowerUserPermissions())
            loadLookupLists();

        loadAcknowledge();
    }
};

DelayedDataLoader::DelayedDataLoader(SystemContext* systemContext, QObject* parent):
    QObject(parent),
    SystemContextAware(systemContext),
    d(new Private{.q=this})
{
    connect(accessController(), &AccessController::userChanged, this,
        [this]()
        {
            d->cancelRequests();
            if (accessController()->user())
                d->loadData();
        });
}

DelayedDataLoader::~DelayedDataLoader()
{
    d->cancelRequests();
}

} // namespace nx::vms::client::desktop
