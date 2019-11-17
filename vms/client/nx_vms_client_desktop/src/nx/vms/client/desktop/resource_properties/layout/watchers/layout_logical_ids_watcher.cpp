#include "layout_logical_ids_watcher.h"
#include "../redux/layout_settings_dialog_store.h"

#include <set>

#include <QtCore/QHash>

#include <common/common_module_aware.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_changes_listener.h>
#include <core/resource_management/resource_pool.h>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

class LayoutLogicalIdsWatcher::Private:
    public QObject,
    public QnCommonModuleAware
{
public:
    Private(LayoutSettingsDialogStore* store, QnCommonModule* commonModule):
        QObject(),
        QnCommonModuleAware(commonModule),
        m_store(store)
    {
        if (!NX_ASSERT(store && commonModule))
            return;

        m_resourceListener = new QnResourceChangesListener(this);
        m_resourceListener->connectToResources<QnLayoutResource>(&QnLayoutResource::logicalIdChanged,
            [this](const QnLayoutResourcePtr& layout) { update(layout); });

        for (const auto& layout: resourcePool()->getResources<QnLayoutResource>())
            update(layout);
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
        if (layout->hasFlags(Qn::ResourceFlag::removed))
        {
            if (m_excludedLayout == layout)
                m_excludedLayout.reset();
            else if (m_logicalIds.remove(layout))
                updateStore();
        }
        else
        {
            const int logicalId = layout->logicalId();
            if (logicalId == 0)
            {
                if (m_logicalIds.remove(layout))
                    updateStore();
            }
            else if (layout != m_excludedLayout)
            {
                m_logicalIds.insert(layout, logicalId);
                updateStore();
            }
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
    QnResourceChangesListener* m_resourceListener = nullptr;
};

LayoutLogicalIdsWatcher::LayoutLogicalIdsWatcher(
    LayoutSettingsDialogStore* store,
    QnCommonModule* commonModule,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(store, commonModule))
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
