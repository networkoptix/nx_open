// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_logical_ids_watcher.h"

#include <set>

#include <QtCore/QHash>

#include <core/resource/layout_resource.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>

#include "../flux/layout_settings_dialog_store.h"

namespace nx::vms::client::desktop {

class LayoutLogicalIdsWatcher::Private:
    public QObject
{
public:
    Private(
        LayoutSettingsDialogStore* store,
        QnResourcePool* resourcePool,
        QnCommonMessageProcessor* messageProcessor)
        :
        QObject(),
        m_store(store)
    {
        if (!NX_ASSERT(store))
            return;

        auto resourceListener = new core::SessionResourcesSignalListener<QnLayoutResource>(
            resourcePool, messageProcessor, this);

        resourceListener->setOnAddedHandler(
            [this](const QnLayoutResourceList& layouts)
            {
                bool needToUpdateStore = false;
                for (const auto& layout: layouts)
                {
                    if (const int logicalId = layout->logicalId(); logicalId > 0)
                    {
                        m_logicalIds.insert(layout, logicalId);
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
                    if (m_excludedLayout == layout)
                        m_excludedLayout.reset();
                    else if (m_logicalIds.remove(layout))
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

    QnLayoutResourcePtr excludedLayout() const
    {
        return m_excludedLayout;
    }

    void setExcludedLayout(const QnLayoutResourcePtr& value)
    {
        if (m_excludedLayout == value)
            return;

        const auto oldExcludedLayout = m_excludedLayout;
        m_excludedLayout = value;

        if (oldExcludedLayout)
            update(oldExcludedLayout);

        if (m_excludedLayout)
            update(m_excludedLayout);
    }

private:
    void update(const QnLayoutResourcePtr& layout)
    {
        const int logicalId = layout->logicalId();
        if (logicalId == 0 || layout == m_excludedLayout)
        {
            if (m_logicalIds.remove(layout))
                updateStore();
        }
        else
        {
            m_logicalIds.insert(layout, logicalId);
            updateStore();
        }
    }

    void updateStore()
    {
        if (!m_store)
            return;

        std::set<int> logicalIds;
        for (const int logicalId: m_logicalIds)
            logicalIds.insert(logicalId);

        m_store->setOtherLogicalIds(logicalIds);
    }

private:
    QPointer<LayoutSettingsDialogStore> m_store;
    QHash<QnLayoutResourcePtr, int> m_logicalIds;
    QnLayoutResourcePtr m_excludedLayout;
};

LayoutLogicalIdsWatcher::LayoutLogicalIdsWatcher(
    LayoutSettingsDialogStore* store,
    QnResourcePool* resourcePool,
    QnCommonMessageProcessor* messageProcessor,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(store, resourcePool, messageProcessor))
{
}

LayoutLogicalIdsWatcher::~LayoutLogicalIdsWatcher()
{
    // Required here for forward-declared scoped pointer destruction.
}

QnLayoutResourcePtr LayoutLogicalIdsWatcher::excludedLayout() const
{
    return d->excludedLayout();
}

void LayoutLogicalIdsWatcher::setExcludedLayout(const QnLayoutResourcePtr& value)
{
    d->setExcludedLayout(value);
}

} // namespace nx::vms::client::desktop
