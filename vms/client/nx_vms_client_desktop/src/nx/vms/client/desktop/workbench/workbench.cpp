// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench.h"

#include <array>

#include <QtCore/QScopedValueRollback>

#include <core/resource/file_layout_resource.h>
#include <core/resource/layout_reader.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_access/resource_access_filter.h>
#include <core/resource_management/resource_pool.h>
#include <finders/systems_finder.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/utils/qt_helpers.h>
#include <nx/utils/std/algorithm.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/access/caching_access_controller.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/cross_system/cross_system_layout_resource.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource/layout_password_management.h>
#include <nx/vms/client/desktop/resource/layout_snapshot_manager.h>
#include <nx/vms/client/desktop/resource/resource_access_manager.h>
#include <nx/vms/client/desktop/resource/unified_resource_pool.h>
#include <nx/vms/client/desktop/showreel/showreel_actions_handler.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_tab_bar/system_tab_bar_model.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/utils/webengine_profile_manager.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/common/showreel/showreel_manager.h>
#include <nx/vms/common/system_settings.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/workbench/extensions/workbench_stream_synchronizer.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_layout_synchronizer.h>
#include <utils/common/checked_cast.h>
#include <utils/common/delayed.h>
#include <utils/common/util.h>
#include <utils/web_downloader.h>

#include "layouts/special_layout.h"

namespace nx::vms::client::desktop {

using namespace ui;
using namespace ui::workbench;

namespace {

static const QString kWorkbenchDataKey = "workbenchState";

static const QSizeF kDefaultCellSize(
    Workbench::kUnitSize,
    Workbench::kUnitSize / QnLayoutResource::kDefaultCellAspectRatio);

QnWorkbenchItem* bestItemForRole(
    const std::array<QnWorkbenchItem*, Qn::ItemRoleCount>& itemByRole,
    Qn::ItemRole role,
    const QnWorkbenchItem* ignoredItem = nullptr)
{
    for (int i = 0; i != role; i++)
    {
        if (itemByRole[i] && itemByRole[i] != ignoredItem)
            return itemByRole[i];
    }

    return nullptr;
}

class StateDelegate: public ClientStateDelegate
{
public:
    StateDelegate(Workbench* workbench): m_workbench(workbench) {}

    virtual bool loadState(const DelegateState& state,
        SubstateFlags flags,
        const StartupParameters& /*params*/) override
    {
        if (!flags.testFlag(ClientStateDelegate::Substate::systemSpecificParameters))
            return false;

        // We can't restore Workbench state until all resources are loaded.
        m_state.currentLayoutId = QnUuid(state.value(kCurrentLayoutId).toString());
        m_state.runningTourId = QnUuid(state.value(kRunningTourId).toString());
        QJson::deserialize(state.value(kLayoutUuids), &m_state.layoutUuids);
        QJson::deserialize(state.value(kUnsavedLayouts), &m_state.unsavedLayouts);
        return true;
    }

    virtual void saveState(DelegateState* state, SubstateFlags flags) override
    {
        if (flags.testFlag(ClientStateDelegate::Substate::systemSpecificParameters))
        {
            m_state = {};
            m_workbench->submit(m_state);

            DelegateState result;
            result[kCurrentLayoutId] = m_state.currentLayoutId.toString();
            result[kRunningTourId] = m_state.runningTourId.toString();
            QJson::serialize(m_state.layoutUuids, &result[kLayoutUuids]);
            QJson::serialize(m_state.unsavedLayouts, &result[kUnsavedLayouts]);
            *state = result;
        }
    }

    virtual void createInheritedState(DelegateState* /*state*/,
        SubstateFlags /*flags*/,
        const QStringList& /*resources*/) override
    {
        // Just return an empty current state without saving and modyfying
        return;
    }

    virtual void forceLoad() override { updateWorkbench(); }
    void updateWorkbench() { m_workbench->update(m_state); }

private:
    WorkbenchState m_state;
    QPointer<Workbench> m_workbench;

    const QString kCurrentLayoutId = "currentLayoutId";
    const QString kRunningTourId = "runningTourId";
    const QString kLayoutUuids = "layoutUids";
    const QString kUnsavedLayouts = "unsavedLayouts";
};

class WorkbenchLayout: public QnWorkbenchLayout
{
public:
    WorkbenchLayout(const LayoutResourcePtr& resource):
        QnWorkbenchLayout(resource)
    {
    }
};

} // namespace

struct Workbench::Private
{
    /** Current layout. */
    QnWorkbenchLayout* currentLayout = nullptr;

    /** List of this workbench's layouts. */
    std::vector<std::unique_ptr<QnWorkbenchLayout>> layouts;

    /** Grid mapper of this workbench. */
    std::unique_ptr<QnWorkbenchGridMapper> mapper = std::make_unique<QnWorkbenchGridMapper>(
        kDefaultCellSize);

    /** Items by role. */
    std::array<QnWorkbenchItem*, Qn::ItemRoleCount> itemByRole = {};

    /** Stored dummy layout. It is used to ensure that current layout is never nullptr. */
    std::unique_ptr<QnWorkbenchLayout> dummyLayout =
        std::make_unique<WorkbenchLayout>(LayoutResourcePtr(new LayoutResource()));

    /** Whether current layout is being changed. */
    bool inLayoutChangeProcess = false;

    /** Whether workbench is in session restore process. */
    bool inSessionRestoreProcess = false;

    std::shared_ptr<StateDelegate> stateDelegate;

    int layoutIndex(const LayoutResourcePtr& resource) const
    {
        auto iter = std::find_if(
            layouts.cbegin(),
            layouts.cend(),
            [resource](const auto& layout)
            {
                return NX_ASSERT(layout) && layout->resource() == resource;
            });

        return iter != layouts.cend()
            ? std::distance(layouts.cbegin(), iter)
            : -1;
    }

    /** Remove unsaved local layouts, restore modified ones. */
    void processRemovedLayouts(const QSet<LayoutResourcePtr>& layouts)
    {
        LayoutResourceList rollbackResources;

        for (const auto& layout: layouts)
        {
            auto systemContext = SystemContext::fromResource(layout);

            // Temporary layouts have no system context and should not be processes anyway.
            if (!systemContext)
                continue;

            auto snapshotManager = systemContext->layoutSnapshotManager();

            // Delete unsaved local layouts.
            if (layout->hasFlags(Qn::local)
                && !layout->isFile()
                && !layout->hasFlags(Qn::local_intercom_layout)) //< FIXME: #dfisenko Remove this.
            {
                systemContext->resourcePool()->removeResource(layout);
            }
            else if (snapshotManager->isChanged(layout))
            {
                snapshotManager->restore(layout);
            }
        }
    }

    void handleResourcesRemoved(const QnResourceList& resources)
    {
        if (currentLayout)
        {
            for (const auto& resource: resources)
                currentLayout->removeItems(resource);
        }
    }

    void removeInaccessibleItems(const QnResourcePtr& resource)
    {
        const auto requiredPermission = QnResourceAccessFilter::isShareableMedia(resource)
            ? Qn::ViewContentPermission
            : Qn::ReadPermission;

        if (currentLayout && !ResourceAccessManager::hasPermissions(resource, requiredPermission))
            currentLayout->removeItems(resource);
    }
};

Workbench::Workbench(QObject* parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    d(new Private())
{
    setCurrentLayout(d->dummyLayout.get());

    d->stateDelegate = std::make_shared<StateDelegate>(this);
    appContext()->clientStateHandler()->registerDelegate(kWorkbenchDataKey, d->stateDelegate);

    connect(appContext()->unifiedResourcePool(), &UnifiedResourcePool::resourcesRemoved, this,
        [this](const QnResourceList& resources) { d->handleResourcesRemoved(resources); });

    // Only currenly connected context resources are checked actually.
    const auto cachingController = qobject_cast<CachingAccessController*>(accessController());
    if (NX_ASSERT(cachingController))
    {
        connect(cachingController, &CachingAccessController::permissionsChanged, this,
            [this](const QnResourcePtr& resource) { d->removeInaccessibleItems(resource); });
    }

    // TODO: #sivanov Move to the appropriate place.
    using WebEngineProfileManager = utils::WebEngineProfileManager;
    connect(WebEngineProfileManager::instance(),
        &WebEngineProfileManager::downloadRequested,
        this,
        [this](QObject* item) { utils::WebDownloader::download(item, context()); });
}

Workbench::~Workbench()
{
    appContext()->clientStateHandler()->unregisterDelegate(kWorkbenchDataKey);
}

WindowContext* Workbench::windowContext() const
{
    return appContext()->mainWindowContext();
}

void Workbench::clear()
{
    setCurrentLayout(nullptr);

    QSet<LayoutResourcePtr> removedLayouts;

    for (const auto& layout: d->layouts)
        removedLayouts.insert(layout->resource());

    d->layouts.clear();
    d->processRemovedLayouts(removedLayouts);

    emit layoutsChanged();
}

QnWorkbenchLayout* Workbench::currentLayout() const
{
    return d->currentLayout;
}

LayoutResourcePtr Workbench::currentLayoutResource() const
{
    return currentLayout()->resource();
}

QnWorkbenchLayout* Workbench::layout(int index) const
{
    if (index < 0 || index >= d->layouts.size())
    {
        NX_ASSERT(false, "Invalid layout index '%1'.", index);
        return nullptr;
    }

    return d->layouts[index].get();
}

QnWorkbenchLayout* Workbench::layout(const LayoutResourcePtr& resource)
{
    auto index = d->layoutIndex(resource);
    return index >= 0
        ? d->layouts[index].get()
        : nullptr;
}

const std::vector<std::unique_ptr<QnWorkbenchLayout>>& Workbench::layouts() const
{
    return d->layouts;
}

QnWorkbenchLayout* Workbench::addLayout(const LayoutResourcePtr& resource)
{
    return insertLayout(resource, d->layouts.size());
}

QnWorkbenchLayout* Workbench::insertLayout(const LayoutResourcePtr& resource, int index)
{
    auto existing = layout(resource);
    if (!NX_ASSERT(!existing, "Layout resource already exists on workbench"))
        return existing;

    // Silently fix index.
    index = std::clamp<int>(0, index, d->layouts.size());
    auto iter = d->layouts.cbegin() + index;

    const bool isSpecial = resource->data(Qn::IsSpecialLayoutRole).toBool();
    std::unique_ptr<QnWorkbenchLayout> layout;
    if (isSpecial)
        layout = std::make_unique<SpecialLayout>(windowContext(), resource);
    else
        layout = std::make_unique<WorkbenchLayout>(resource);

    auto result = layout.get();
    d->layouts.insert(iter, std::move(layout));

    NX_ASSERT(result == d->layouts[index].get());

    emit layoutsChanged();
    return result;
}

QnWorkbenchLayout* Workbench::replaceLayout(
    const LayoutResourcePtr& replaceableLayout,
    const LayoutResourcePtr& newLayout)
{
    // Replace layout only if it is already on the workbench.
    if (const auto workbenchLayout = layout(replaceableLayout))
    {
        const bool isCurrentLayout = currentLayout() == workbenchLayout;
        const auto savedSyncState = isCurrentLayout
            ? windowContext()->streamSynchronizer()->state()
            : workbenchLayout->streamSynchronizationState();

        const int index = layoutIndex(workbenchLayout);
        const auto newWorkbenchLayout = insertLayout(newLayout, index);
        newWorkbenchLayout->setStreamSynchronizationState(savedSyncState);

        if (isCurrentLayout)
            setCurrentLayoutIndex(index);

        removeLayout(index + 1);

        return newWorkbenchLayout;
    }

    return {};
}

void Workbench::removeLayout(int index)
{
    if (!NX_ASSERT(index >= 0 && index < d->layouts.size()))
        return;

    auto layout = d->layouts[index].get();
    QSet<LayoutResourcePtr> removedLayouts{layout->resource()};

    // Update current layout if it's being removed.
    if (layout == d->currentLayout)
    {
        QnWorkbenchLayout* newCurrentLayout = nullptr;

        int newCurrentIndex = index;
        newCurrentIndex++;
        if (newCurrentIndex >= d->layouts.size())
            newCurrentIndex -= 2;
        if (newCurrentIndex >= 0)
            newCurrentLayout = d->layouts[newCurrentIndex].get();

        setCurrentLayout(newCurrentLayout);
    }

    d->layouts.erase(d->layouts.begin() + index);
    d->processRemovedLayouts(removedLayouts);

    emit layoutsChanged();
}

void Workbench::removeLayout(const LayoutResourcePtr& resource)
{
    auto index = d->layoutIndex(resource);
    if (index >= 0)
        removeLayout(index);
}

void Workbench::removeLayouts(const LayoutResourceList& resources)
{
    const auto removedLayouts = nx::utils::toQSet(resources);
    NX_ASSERT(removedLayouts.size() == resources.size());
    const bool updateCurrentLayout = (d->currentLayout
        && removedLayouts.contains(d->currentLayout->resource()));

    if (updateCurrentLayout)
    {
        int newCurrentLayoutIndex = 0;
        while (newCurrentLayoutIndex < d->layouts.size()
            && removedLayouts.contains(d->layouts[newCurrentLayoutIndex]->resource()))
        {
            ++newCurrentLayoutIndex;
        }

        if (newCurrentLayoutIndex < d->layouts.size())
            setCurrentLayout(d->layouts[newCurrentLayoutIndex].get());
        else
            setCurrentLayout(nullptr);
    }

    const size_t removed = nx::utils::erase_if(d->layouts,
        [&removedLayouts](const std::unique_ptr<QnWorkbenchLayout>& layout)
        {
            return removedLayouts.contains(layout->resource());
        });

    if (removed == 0)
    {
        NX_ASSERT(!updateCurrentLayout);
        return;
    }

    d->processRemovedLayouts(removedLayouts);
    emit layoutsChanged();
}

void Workbench::moveLayout(QnWorkbenchLayout* layout, int index)
{
    if (!NX_ASSERT(layout))
        return;

    int currentIndex = d->layoutIndex(layout->resource());
    if (!NX_ASSERT(currentIndex >= 0, "Layout is not in the list"))
        return;

    // Silently fix index.
    index = std::clamp<int>(0, index, d->layouts.size());
    if (currentIndex == index)
        return;

    std::swap(d->layouts[currentIndex], d->layouts[index]);
    emit layoutsChanged();
}

int Workbench::layoutIndex(QnWorkbenchLayout* layout) const
{
    return d->layoutIndex(layout->resource());
}

void Workbench::setCurrentLayoutIndex(int index)
{
    if (d->layouts.empty())
        return;

    setCurrentLayout(d->layouts[std::clamp<int>(0, index, d->layouts.size())].get());
}

void Workbench::addSystem(QnUuid systemId, const LogonData& logonData)
{
    if (!ini().enableMultiSystemTabBar)
        return;

    auto systemDescription = qnSystemsFinder->getSystem(systemId.toString());
    if (systemDescription.isNull())
        systemDescription = qnSystemsFinder->getSystem(systemId.toSimpleString());

    if (!NX_ASSERT(systemDescription, "Can't find description for system: %1", systemId))
        return;

    windowContext()->systemTabBarModel()->addSystem(systemDescription, logonData);
    emit currentSystemChanged(systemDescription);
}

void Workbench::addSystem(const QString& systemId, const LogonData& logonData)
{
    if (!ini().enableMultiSystemTabBar)
        return;

    auto systemDescription = qnSystemsFinder->getSystem(systemId);

    NX_ASSERT(systemDescription, "Can't find description for system: %1", systemId);

    windowContext()->systemTabBarModel()->addSystem(systemDescription, logonData);
    emit currentSystemChanged(systemDescription);
}

void Workbench::removeSystem(const QnSystemDescriptionPtr& systemDescription)
{
    windowContext()->systemTabBarModel()->removeSystem(systemDescription);
}

void Workbench::removeSystem(const QnUuid& systemId)
{
    if (!systemId.isNull())
        windowContext()->systemTabBarModel()->removeSystem(systemId.toSimpleString());
}

void Workbench::setCurrentLayout(QnWorkbenchLayout* layout)
{
    if (!layout)
        layout = d->dummyLayout.get();

    if (d->currentLayout == layout)
        return;

    QString password;
    if (NX_ASSERT(layout))
    {
        auto resource = layout->resource().dynamicCast<QnFileLayoutResource>();
        if (resource && resource->requiresPassword())
        {
            // When opening encrypted layout, ask for the password and remove layout if
            // unsuccessful.
            password = layout::askPassword(resource, mainWindowWidget());
            if (password.isEmpty())
            {
                // The layout must be removed after opening, not during opening. Otherwise, the
                // tabbar invariant will be violated.
                d->currentLayout = layout;
                executeDelayedParented(
                    [this, layout]
                    {
                        menu()->trigger(action::CloseLayoutAction,
                            QnWorkbenchLayoutList() << layout);
                    },
                    this);
                return;
            }

            layout::reloadFromFile(resource, password);
            layout->layoutSynchronizer()->update();

            auto systemContext = SystemContext::fromResource(resource);
            if (NX_ASSERT(systemContext))
                systemContext->layoutSnapshotManager()->store(resource);
        }
    }

    d->inLayoutChangeProcess = true;
    emit currentLayoutAboutToBeChanged();

    // Clean up old layout.
    // It may be nullptr only when this function is called from constructor.
    qreal oldCellAspectRatio = -1.0;
    qreal oldCellSpacing = -1.0;
    if (d->currentLayout)
    {
        oldCellAspectRatio = d->currentLayout->cellAspectRatio();
        oldCellSpacing = d->currentLayout->cellSpacing();

        const auto activeItem = d->itemByRole[Qn::ActiveRole];
        d->currentLayout->setData(Qn::LayoutActiveItemRole,
            activeItem ? QVariant::fromValue(activeItem->uuid()) : QVariant());

        for (int i = 0; i < Qn::ItemRoleCount; i++)
            setItem(static_cast<Qn::ItemRole>(i), nullptr);

        d->currentLayout->disconnect(this);
    }

    /* Prepare new layout. */
    d->currentLayout = layout;
    if (!d->currentLayout)
    {
        d->dummyLayout->clear();
        d->currentLayout = d->dummyLayout.get();
    }

    // Set up a new layout.
    const auto activeItemUuid = d->currentLayout->data(Qn::LayoutActiveItemRole).value<QnUuid>();
    if (!activeItemUuid.isNull())
        setItem(Qn::ActiveRole, d->currentLayout->item(activeItemUuid));

    connect(
        d->currentLayout, &QnWorkbenchLayout::itemAdded, this, &Workbench::at_layout_itemAdded);
    connect(d->currentLayout,
        &QnWorkbenchLayout::itemRemoved,
        this,
        &Workbench::at_layout_itemRemoved);
    connect(d->currentLayout,
        &QnWorkbenchLayout::cellAspectRatioChanged,
        this,
        &Workbench::at_layout_cellAspectRatioChanged);
    connect(d->currentLayout,
        &QnWorkbenchLayout::cellSpacingChanged,
        this,
        &Workbench::at_layout_cellSpacingChanged);

    const qreal newCellAspectRatio = d->currentLayout->cellAspectRatio();
    const qreal newCellSpacing = d->currentLayout->cellSpacing();

    if (!qFuzzyEquals(newCellAspectRatio, oldCellAspectRatio))
        at_layout_cellAspectRatioChanged();

    if (!qFuzzyEquals(newCellSpacing, oldCellSpacing))
        at_layout_cellSpacingChanged();

    updateActiveRoleItem();

    d->inLayoutChangeProcess = false;

    emit currentLayoutChanged();
}

QnWorkbenchGridMapper* Workbench::mapper() const
{
    return d->mapper.get();
}

QnWorkbenchItem* Workbench::item(Qn::ItemRole role)
{
    NX_ASSERT(role >= 0 && role < Qn::ItemRoleCount);

    return d->itemByRole[role];
}

void Workbench::setItem(Qn::ItemRole role, QnWorkbenchItem* item)
{
    NX_ASSERT(role >= 0 && role < Qn::ItemRoleCount);
    if (d->itemByRole[role] == item)
        return;

    // Do not set focus at the item if it is not allowed.
    const auto widget = display()->widget(item);
    if (widget && !widget->options().testFlag(QnResourceWidget::AllowFocus)
        && role > Qn::SingleSelectedRole)
    {
        return;
    }

    // Delayed clicks may lead to very interesting scenarios. Just ignore.
    bool validLayout = !item || item->layout() == d->currentLayout;
    if (!validLayout)
        return;

    emit itemAboutToBeChanged(role);
    d->itemByRole[role] = item;
    emit itemChanged(role);

    /* If we are changing focus, make sure raised item will be un-raised. */
    if (role > Qn::RaisedRole && d->itemByRole[Qn::RaisedRole]
        && d->itemByRole[Qn::RaisedRole] != item)
    {
        setItem(Qn::RaisedRole, nullptr);
        return; /* Active and central roles will be updated in the recursive call */
    }

    /* Update items for derived roles. */
    if (role < Qn::ActiveRole)
        updateActiveRoleItem();

    if (role < Qn::CentralRole)
        updateCentralRoleItem();
}

void Workbench::updateSingleRoleItem()
{
    if (d->currentLayout->items().size() == 1)
    {
        setItem(Qn::SingleRole, *d->currentLayout->items().begin());
    }
    else
    {
        setItem(Qn::SingleRole, nullptr);
    }
}

void Workbench::updateActiveRoleItem(const QnWorkbenchItem* removedItem)
{
    auto activeItem = bestItemForRole(d->itemByRole, Qn::ActiveRole, removedItem);
    if (activeItem)
    {
        setItem(Qn::ActiveRole, activeItem);
        return;
    }

    if (d->itemByRole[Qn::ActiveRole] && d->itemByRole[Qn::ActiveRole] != removedItem)
        return;

    // Try to select the same camera as just removed (if any left).
    QSet<QnWorkbenchItem*> sameResourceItems;
    if (removedItem)
        sameResourceItems = d->currentLayout->items(removedItem->resource());

    const auto& itemsToSelectFrom =
        sameResourceItems.empty() ? d->currentLayout->items() : sameResourceItems;

    if (!itemsToSelectFrom.isEmpty())
        activeItem = *itemsToSelectFrom.begin();

    setItem(Qn::ActiveRole, activeItem);
}

void Workbench::updateCentralRoleItem()
{
    setItem(Qn::CentralRole, bestItemForRole(d->itemByRole, Qn::CentralRole));
}

void Workbench::update(const WorkbenchState& state)
{
    QScopedValueRollback<bool> rollback(d->inSessionRestoreProcess, true);
    clear();

    for (const auto& id: state.layoutUuids)
    {
        if (const auto layout = resourcePool()->getResourceById<LayoutResource>(id))
        {
            addLayout(layout);
            continue;
        }

        if (const auto layout = appContext()->cloudLayoutsPool()->
            getResourceById<CrossSystemLayoutResource>(id))
        {
            addLayout(layout);
            continue;
        }

        if (const auto videoWall = resourcePool()->getResourceById<QnVideoWallResource>(id))
        {
            menu()->trigger(action::OpenVideoWallReviewAction, videoWall);
            continue;
        }

        const auto showreel = systemContext()->showreelManager()->showreel(id);
        if (showreel.isValid())
        {
            menu()->trigger(action::ReviewShowreelAction, {Qn::UuidRole, id});
            continue;
        }
    }

    if (context()->user())
    {
        auto initialFillLayoutFromState =
            [](const LayoutResourcePtr& layout, const WorkbenchState::UnsavedLayout& state)
            {
                layout->addFlags(Qn::local);
                layout->setParentId(state.parentId);
                layout->setIdUnsafe(state.id);
                layout->setName(state.name);
            };

        const auto userId = context()->user()->getId();
        for (const auto& stateLayout: state.unsavedLayouts)
        {
            LayoutResourcePtr layoutResource;
            if (stateLayout.isCrossSystem)
            {
                layoutResource = appContext()->cloudLayoutsPool()->
                    getResourceById<LayoutResource>(stateLayout.id);
                if (!layoutResource)
                {
                    layoutResource.reset(new CrossSystemLayoutResource());
                    initialFillLayoutFromState(layoutResource, stateLayout);
                    appContext()->cloudLayoutsPool()->addResource(layoutResource);
                }
            }
            else
            {
                if (stateLayout.parentId != userId)
                    continue;

                layoutResource = resourcePool()->getResourceById<LayoutResource>(stateLayout.id);
                if (!layoutResource)
                {
                    layoutResource.reset(new LayoutResource());
                    initialFillLayoutFromState(layoutResource, stateLayout);
                    resourcePool()->addResource(layoutResource);
                }
            }

            layoutResource->setCellSpacing(stateLayout.cellSpacing);
            layoutResource->setCellAspectRatio(stateLayout.cellAspectRatio);
            layoutResource->setBackgroundImageFilename(stateLayout.backgroundImageFilename);
            layoutResource->setBackgroundOpacity(stateLayout.backgroundOpacity);
            layoutResource->setBackgroundSize(stateLayout.backgroundSize);

            common::LayoutItemDataList items;
            for (const auto& itemData: stateLayout.items)
            {
                items.append(itemData);
            }
            layoutResource->setItems(items);

            if (QnWorkbenchLayout* layout = this->layout(layoutResource))
                layout->update(layoutResource);
            else
                addLayout(layoutResource);
        }
    }

    if (!state.currentLayoutId.isNull())
    {
        auto restoreCurrentLayout =
            [this](const QnResourcePool* pool, const QnUuid id)
            {
                const LayoutResourcePtr& layout = pool->getResourceById<LayoutResource>(id);
                if (!layout)
                    return false;

                if (const auto wbLayout = this->layout(layout))
                {
                    setCurrentLayout(wbLayout);
                    return true;
                }

                return false;
            };

        if (!restoreCurrentLayout(resourcePool(), state.currentLayoutId))
            restoreCurrentLayout(appContext()->cloudLayoutsPool(), state.currentLayoutId);
    }

    // Empty workbench is not supported correctly.
    // Create a new layout in the case there are no opened layouts in the workbench state.
    if (layouts().empty())
        menu()->trigger(ui::action::OpenNewTabAction);

    if (currentLayoutIndex() == -1)
        setCurrentLayoutIndex(layouts().size() - 1);

    if (!state.runningTourId.isNull())
    {
        menu()->trigger(action::ToggleShowreelModeAction, {Qn::UuidRole, state.runningTourId});
    }
}

void Workbench::submit(WorkbenchState& state, bool forceIncludeEmptyLayouts)
{
    auto isLayoutSupported = [](const LayoutResourcePtr& layout, bool allowLocals)
    {
        // Support showreels.
        if (layout->isShowreelReviewLayout())
            return true;

        if (layout->data().contains(Qn::VideoWallResourceRole))
            return true;

        // Ignore other service layouts, e.g. videowall control layouts.
        if ((layout->hasFlags(Qn::local) && !allowLocals) || layout->isServiceLayout())
            return false;

        // Ignore temporary layouts such as preview search or audit trail.
        if (!layout->systemContext())
            return false;

        return true;
    };

    auto sourceId = [](const LayoutResourcePtr& layout)
    {
        if (const auto videoWall =
                layout->data(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>())
        {
            return videoWall->getId();
        }

        const auto tourId = layout->data(Qn::ShowreelUuidRole).value<QnUuid>();
        return tourId.isNull() ? layout->getId() : tourId;
    };

    {
        auto currentResource = currentLayout()->resource();
        if (currentResource && isLayoutSupported(currentResource, /*allowLocals*/ true))
            state.currentLayoutId = sourceId(currentResource);
    }

    for (const auto& layout: d->layouts)
    {
        auto resource = layout->resource();
        if (resource)
        {
            if (isLayoutSupported(resource, /*allowLocals*/ false))
                state.layoutUuids.push_back(sourceId(resource));

            if (resource->hasFlags(Qn::local) &&
                (!resource->getItems().empty() || forceIncludeEmptyLayouts))
            {
                WorkbenchState::UnsavedLayout unsavedLayout;
                unsavedLayout.id = resource->getId();
                unsavedLayout.parentId = resource->getParentId();
                unsavedLayout.name = layout->name();
                unsavedLayout.cellSpacing = resource->cellSpacing();
                unsavedLayout.cellAspectRatio = resource->cellAspectRatio();
                unsavedLayout.backgroundImageFilename = resource->backgroundImageFilename();
                unsavedLayout.backgroundOpacity = resource->backgroundOpacity();
                unsavedLayout.backgroundSize = resource->backgroundSize();
                unsavedLayout.isCrossSystem = resource->hasFlags(Qn::cross_system);

                for (const auto& itemData: resource->getItems().values())
                {
                    unsavedLayout.items << itemData;
                }
                state.unsavedLayouts << unsavedLayout;
            }
        }
    }

    state.runningTourId = context()->instance<ShowreelActionsHandler>()->runningShowreel();
}

void Workbench::applyLoadedState()
{
    auto model = windowContext()->systemTabBarModel();
    auto modelIndex = model->findSystem(systemContext()->localSystemId());
    if (modelIndex.isValid())
    {
        const auto workbenchState = modelIndex.data(Qn::WorkbenchStateRole).value<WorkbenchState>();
        if (!workbenchState.layoutUuids.empty() || !workbenchState.unsavedLayouts.empty())
        {
            update(workbenchState);
            return;
        }
    }
    d->stateDelegate->updateWorkbench();
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void Workbench::at_layout_itemAdded(QnWorkbenchItem* item)
{
    emit currentLayoutItemAdded(item);
    emit currentLayoutItemsChanged();
    updateSingleRoleItem();
}

void Workbench::at_layout_itemRemoved(QnWorkbenchItem* item)
{
    emit currentLayoutItemRemoved(item);
    emit currentLayoutItemsChanged();

    updateSingleRoleItem();
    updateActiveRoleItem(item);

    for (int i = 0; i < Qn::ItemRoleCount; i++)
    {
        if (item == d->itemByRole[i])
            setItem(static_cast<Qn::ItemRole>(i), nullptr);
    }
}

void Workbench::at_layout_cellAspectRatioChanged()
{
    qreal cellAspectRatio = d->currentLayout->hasCellAspectRatio()
        ? d->currentLayout->cellAspectRatio()
        : QnLayoutResource::kDefaultCellAspectRatio;

    d->mapper->setCellSize(kUnitSize, kUnitSize / cellAspectRatio);

    emit cellAspectRatioChanged();
}

void Workbench::at_layout_cellSpacingChanged()
{
    d->mapper->setSpacing(d->currentLayout->cellSpacing() * kUnitSize);

    emit cellSpacingChanged();
}

bool Workbench::isInLayoutChangeProcess() const
{
    return d->inLayoutChangeProcess;
}

bool Workbench::isInSessionRestoreProcess() const
{
    return d->inSessionRestoreProcess;
}

} // namespace nx::vms::client::desktop
