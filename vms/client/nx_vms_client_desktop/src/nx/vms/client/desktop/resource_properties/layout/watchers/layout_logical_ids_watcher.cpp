// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_logical_ids_watcher.h"

#include <set>

#include <QtCore/QHash>

#include <core/resource/layout_resource.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/desktop/system_context.h>

#include "../flux/layout_settings_dialog_store.h"

namespace nx::vms::client::desktop {

struct LayoutLogicalIdsWatcher::Private
{
    LayoutLogicalIdsWatcher* const q;
    LayoutSettingsDialogStore* const store;

    QHash<QnLayoutResourcePtr, int> logicalIds;
    QnLayoutResourcePtr excludedLayout;

    Private(
        LayoutLogicalIdsWatcher* owner,
        LayoutSettingsDialogStore* store)
        :
        q(owner),
        store(store)
    {
        auto resourceListener = new core::SessionResourcesSignalListener<QnLayoutResource>(
            q->systemContext(),
            q);

        resourceListener->setOnAddedHandler(
            [this](const QnLayoutResourceList& layouts)
            {
                bool needToUpdateStore = false;
                for (const auto& layout: layouts)
                {
                    if (const int logicalId = layout->logicalId(); logicalId > 0)
                    {
                        logicalIds.insert(layout, logicalId);
                        needToUpdateStore = true;
                    }
                }
                if (needToUpdateStore)
                    updateStore();
            });

        resourceListener->setOnRemovedHandler(
            [this](const QnLayoutResourceList& layouts)
            {
                bool needToUpdateStore = false;
                for (const auto& layout: layouts)
                {
                    if (excludedLayout == layout)
                        excludedLayout.reset();
                    else if (logicalIds.remove(layout))
                        needToUpdateStore = true;
                }
                if (needToUpdateStore)
                    updateStore();
            });

        resourceListener->addOnSignalHandler(
            &QnLayoutResource::logicalIdChanged,
            [this](const QnLayoutResourcePtr& layout) { update(layout); });

        resourceListener->start();
    }

    void setExcludedLayout(const QnLayoutResourcePtr& value)
    {
        excludedLayout = value;
        update(excludedLayout);
    }

    void update(const QnLayoutResourcePtr& layout)
    {
        const int logicalId = layout->logicalId();
        if (logicalId == 0 || layout == excludedLayout)
        {
            if (logicalIds.remove(layout))
                updateStore();
        }
        else
        {
            logicalIds.insert(layout, logicalId);
            updateStore();
        }
    }

    void updateStore()
    {
        std::set<int> value{logicalIds.cbegin(), logicalIds.cend()};
        store->setOtherLogicalIds(value);
    }
};

LayoutLogicalIdsWatcher::LayoutLogicalIdsWatcher(
    LayoutSettingsDialogStore* store,
    QnLayoutResourcePtr excludedLayout,
    QObject* parent)
    :
    base_type(parent),
    SystemContextAware(SystemContext::fromResource(excludedLayout)),
    d(new Private(this, store))
{
    d->setExcludedLayout(excludedLayout);
}

LayoutLogicalIdsWatcher::~LayoutLogicalIdsWatcher()
{
    // Required here for forward-declared scoped pointer destruction.
}

} // namespace nx::vms::client::desktop
