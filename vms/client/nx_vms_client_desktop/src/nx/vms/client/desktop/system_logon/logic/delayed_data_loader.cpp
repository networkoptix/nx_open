// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "delayed_data_loader.h"

#include <api/server_rest_connection.h>
#include <client/client_message_processor.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/lookup_list_data.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/lookup_lists/lookup_list_manager.h>
#include <ui/workbench/workbench_access_controller.h>

namespace nx::vms::client::desktop {

struct DelayedDataLoader::Private
{
    DelayedDataLoader* const q;
    std::optional<rest::Handle> requestId;

    void cancelRequest()
    {
        if (requestId)
            q->connectedServerApi()->cancelRequest(*requestId);
        requestId = {};
    }

    void loadLookupLists()
    {
        using nx::vms::api::LookupListDataList;

        auto callback = nx::utils::guarded(
            q,
            [this](
                bool /*success*/,
                rest::Handle requestId,
                const rest::ErrorOrData<LookupListDataList>& result)
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
                    return;
                }

                auto lookupLists = std::get<LookupListDataList>(result);
                NX_VERBOSE(this, "Received %1 Lookup Lists entries", lookupLists.size());

                q->lookupListManager()->initialize(std::move(lookupLists));
            });

        requestId = q->connectedServerApi()->getLookupLists(std::move(callback), q->thread());
        NX_VERBOSE(this, "Send get lookup lists request (%1)", *requestId);
    }

    void loadData()
    {
        // While there is only one piece of data, we can keep the architecture simple. Later we
        // will need to implement requests queue as in CloudCrossSystemContextDataLoader class.

        if (ini().lookupLists && q->accessController()->hasPowerUserPermissions())
            loadLookupLists();
    }
};

DelayedDataLoader::DelayedDataLoader(SystemContext* systemContext, QObject* parent):
    QObject(parent),
    SystemContextAware(systemContext),
    d(new Private{.q=this})
{
    connect(accessController(), &QnWorkbenchAccessController::userChanged, this,
        [this](const QnUserResourcePtr& user)
        {
            d->cancelRequest();
            if (user)
                d->loadData();
        });
}

DelayedDataLoader::~DelayedDataLoader()
{
    d->cancelRequest();
}

} // namespace nx::vms::client::desktop
