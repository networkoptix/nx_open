// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "feature_access_watcher.h"

#include <nx/vms/client/core/network/remote_session.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/client/core/network/remote_connection.h>

namespace nx::vms::client::core {

struct FeatureAccessWatcher::Private
{
    FeatureAccessWatcher* const q;

    bool canUseShareBookmark = false;

    void updateCanUseShareBookmark();
};

void FeatureAccessWatcher::Private::updateCanUseShareBookmark()
{
    const bool newValue =
        [this]()
        {
            const auto context = q->systemContext();
            if (!context)
                return false;

            const auto session = context->session();
            const auto connection = session
                ? session->connection()
                : context->connection();
            if (!connection || !connection->connectionInfo().isCloud())
                return false;

            const auto moduleInformation = context->moduleInformation();
            return moduleInformation.version >= utils::SoftwareVersion(6, 1)
                && moduleInformation.organizationId;
        }();

    if (newValue == canUseShareBookmark)
        return;

    canUseShareBookmark = newValue;
    emit q->canUseShareBookmarkChanged();
}

FeatureAccessWatcher::FeatureAccessWatcher(SystemContext* context, QObject* parent):
    QObject(parent),
    SystemContextAware(context),
    d(new Private{.q = this})
{
    const auto updateCanShareBookmarkAvailability =
        [this]() { d->updateCanUseShareBookmark(); };

    connect(context, &SystemContext::remoteIdChanged,
        this, updateCanShareBookmarkAvailability);
    connect(context, &SystemContext::credentialsChanged,
        this, updateCanShareBookmarkAvailability);

    updateCanShareBookmarkAvailability();
}

FeatureAccessWatcher::~FeatureAccessWatcher() = default;

bool FeatureAccessWatcher::canUseShareBookmark() const
{
    return d->canUseShareBookmark;
}

} // namespace nx::vms::client::core
