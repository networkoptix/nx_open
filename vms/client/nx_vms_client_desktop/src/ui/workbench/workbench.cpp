// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench.h"

#include <common/common_module.h>
#include <core/resource/file_layout_resource.h>
#include <core/resource/layout_reader.h>
#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/layout_tour_manager.h>
#include <core/resource_management/resource_pool.h>
#include <finders/systems_finder.h>
#include <nx/fusion/serialization/json_functions.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource/layout_password_management.h>
#include <nx/vms/client/desktop/resource/layout_snapshot_manager.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_tab_bar/system_tab_bar_model.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/utils/webengine_profile_manager.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/handlers/layout_tours_handler.h>
#include <nx/vms/client/desktop/workbench/layouts/layout_factory.h>
#include <nx/vms/client/desktop/workbench/layouts/special_layout.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_layout.h>
#include <utils/common/checked_cast.h>
#include <utils/common/util.h>
#include <utils/web_downloader.h>

#include "workbench_layout_synchronizer.h"

using namespace nx::vms::client::desktop;
using namespace ui;
using namespace ui::workbench;

class QnWorkbench::StateDelegate: public ClientStateDelegate
{
public:
    StateDelegate(QnWorkbench* workbench) : m_workbench(workbench)
    {
    }

    virtual bool loadState(
        const DelegateState& state,
        SubstateFlags flags,
        const StartupParameters& /*params*/) override
    {
        if (!flags.testFlag(ClientStateDelegate::Substate::systemSpecificParameters))
            return false;

        // We can't restore Workbench state until all resources are loaded.
        m_state.currentLayoutId = QnUuid(state.value(kCurrentLayoutId).toString());
        m_state.runningTourId = QnUuid(state.value(kRunningTourId).toString());
        QJson::deserialize(state.value(kLayoutUuids), &m_state.layoutUuids);
        if (ini().enableMultiSystemTabBar)
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
            if (ini().enableMultiSystemTabBar)
                QJson::serialize(m_state.unsavedLayouts, &result[kUnsavedLayouts]);
            *state = result;
        }
    }

    virtual void createInheritedState(
        DelegateState* /*state*/,
        SubstateFlags /*flags*/,
        const QStringList& /*resources*/) override
    {
        // Just return an empty current state without saving and modyfying
        return;
    }

    virtual void forceLoad() override
    {
        updateWorkbench();
    }

    void updateWorkbench()
    {
        m_workbench->update(m_state);
    }

private:
    QnWorkbenchState m_state;
    QPointer<QnWorkbench> m_workbench;

    const QString kCurrentLayoutId = "currentLayoutId";
    const QString kRunningTourId = "runningTourId";
    const QString kLayoutUuids = "layoutUids";
    const QString kUnsavedLayouts = "unsavedLayouts";
};

namespace {

static const QString kWorkbenchDataKey = "workbenchState";

QnWorkbenchItem* bestItemForRole(QnWorkbenchItem** itemByRole, Qn::ItemRole role,
    const QnWorkbenchItem* ignoredItem = nullptr)
{
    for (int i = 0; i != role; i++)
    {
        if (itemByRole[i] && itemByRole[i] != ignoredItem)
            return itemByRole[i];
    }

    return nullptr;
}

void addLayoutCreators(LayoutsFactory* factory)
{
    const auto standardLayoutCreator =
        [](const QnLayoutResourcePtr& resource, QObject* parent) -> QnWorkbenchLayout*
        {
            if (!resource)
                return new QnWorkbenchLayout(resource, parent);

            return resource->data(Qn::IsSpecialLayoutRole).toBool()
                ? new SpecialLayout(resource, parent)
                : new QnWorkbenchLayout(resource, parent);
        };

    factory->addCreator(standardLayoutCreator);
}
} // anonymous namespace


QnWorkbench::QnWorkbench(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_currentLayout(nullptr),
    m_inLayoutChangeProcess(false)
{
    for(int i = 0; i < Qn::ItemRoleCount; i++)
        m_itemByRole[i] = nullptr;

    const auto factory = LayoutsFactory::instance(this);
    addLayoutCreators(factory);

    const QSizeF kDefaultCellSize(kUnitSize, kUnitSize / QnLayoutResource::kDefaultCellAspectRatio);
    m_mapper = new QnWorkbenchGridMapper(kDefaultCellSize, this);

    m_dummyLayout = factory->create(this);
    setCurrentLayout(m_dummyLayout);

    auto delegate = new StateDelegate(this);
    m_stateDelegate = delegate;
    appContext()->clientStateHandler()->registerDelegate(
        kWorkbenchDataKey, std::unique_ptr<StateDelegate>(delegate));

    using WebEngineProfileManager = nx::vms::client::desktop::utils::WebEngineProfileManager;
    connect(
        WebEngineProfileManager::instance(),
        &WebEngineProfileManager::downloadRequested,
        this,
        [this](QObject* item)
        {
            utils::WebDownloader::download(item, context());
        });
}

QnWorkbench::~QnWorkbench()
{
    bool signalsBlocked = blockSignals(false);
    emit aboutToBeDestroyed();
    blockSignals(signalsBlocked);

    appContext()->clientStateHandler()->unregisterDelegate(kWorkbenchDataKey);
    m_dummyLayout = nullptr;
    clear();
}

nx::vms::client::desktop::WindowContext* QnWorkbench::windowContext() const
{
    return appContext()->mainWindowContext();
}

void QnWorkbench::clear()
{
    QnLayoutResourceList resources;
    for (const auto layout: qAsConst(m_layouts))
    {
        if (layout->data(Qn::IsSpecialLayoutRole).isValid() && layout->data(Qn::IsSpecialLayoutRole).toBool())
            continue;

        auto systemContext = SystemContext::fromResource(layout->resource());
        if (NX_ASSERT(systemContext))
            systemContext->layoutSnapshotManager()->restore(layout->resource());
    }

    setCurrentLayout(nullptr);

    blockSignals(true);

    auto layoutsToDestroy = m_layouts;
    for (const auto& layout: layoutsToDestroy)
        removeLayout(layout);

    blockSignals(false);

    emit layoutsChanged();

    qDeleteAll(layoutsToDestroy);
}

QnWorkbenchLayout *QnWorkbench::layout(int index) const {
    if(index < 0 || index >= m_layouts.size()) {
        NX_ASSERT(false, "Invalid layout index '%1'.", index);
        return nullptr;
    }

    return m_layouts[index];
}

void QnWorkbench::addLayout(QnWorkbenchLayout *layout) {
    insertLayout(layout, m_layouts.size());
}

void QnWorkbench::removeSystem(const QnSystemDescriptionPtr& systemDescription)
{
    windowContext()->systemTabBarModel()->removeSystem(systemDescription);
}

void QnWorkbench::removeSystem(const QString& systemId)
{
    windowContext()->systemTabBarModel()->removeSystem(systemId);
}

void QnWorkbench::insertLayout(QnWorkbenchLayout *layout, int index)
{
    if (!NX_ASSERT(layout))
        return;

    if(m_layouts.contains(layout)) {
        NX_ASSERT(false, "Given layout is already in this workbench's layouts list.");
        return;
    }

    /* Silently fix index. */
    index = qBound(0, index, m_layouts.size());

    m_layouts.insert(index, layout);
    connect(layout, SIGNAL(aboutToBeDestroyed()), this, SLOT(at_layout_aboutToBeDestroyed()));

    emit layoutsChanged();
}

void QnWorkbench::removeLayout(int index) {
    removeLayout(layout(index));
}

void QnWorkbench::removeLayout(QnWorkbenchLayout* layout)
{
    if (!NX_ASSERT(layout))
        return;

    if (!m_layouts.contains(layout))
        return; /* Removing a layout that is not there is OK. */

    /* Update current layout if it's being removed. */
    if (layout == m_currentLayout)
    {
        QnWorkbenchLayout *newCurrentLayout = nullptr;

        int newCurrentIndex = m_layouts.indexOf(m_currentLayout);
        newCurrentIndex++;
        if(newCurrentIndex >= m_layouts.size())
            newCurrentIndex -= 2;
        if(newCurrentIndex >= 0)
            newCurrentLayout = m_layouts[newCurrentIndex];

        setCurrentLayout(newCurrentLayout);
    }

    m_layouts.removeOne(layout);
    layout->disconnect(this);

    emit layoutsChanged();
}

void QnWorkbench::moveLayout(QnWorkbenchLayout *layout, int index) {
    if (!NX_ASSERT(layout))
        return;

    if(!m_layouts.contains(layout)) {
        NX_ASSERT(false, "Given layout is not in this workbench's layouts list.");
        return;
    }

    /* Silently fix index. */
    index = qBound(0, index, m_layouts.size() - 1);

    int currentIndex = m_layouts.indexOf(layout);
    if(currentIndex == index)
        return;

    m_layouts.move(currentIndex, index);

    emit layoutsChanged();
}

int QnWorkbench::layoutIndex(QnWorkbenchLayout *layout) const {
    return m_layouts.indexOf(layout);
}

void QnWorkbench::setCurrentLayoutIndex(int index) {
    if(m_layouts.empty())
        return;

    setCurrentLayout(m_layouts[qBound(0, index, m_layouts.size())]);
}

QnWorkbenchLayout* QnWorkbench::findLayout(QnUuid id)
{
    auto it = std::find_if(m_layouts.begin(), m_layouts.end(),
        [&id](QnWorkbenchLayout* item)
        {
            return item->resource()->getId() == id;
        });

    return it == m_layouts.end() ? nullptr : *it;
}

void QnWorkbench::addSystem(QnUuid systemId, const LogonData& logonData)
{
    auto systemDescription = qnSystemsFinder->getSystem(systemId.toString());
    if (systemDescription.isNull())
        systemDescription = qnSystemsFinder->getSystem(systemId.toSimpleString());

    NX_ASSERT(systemDescription);

    windowContext()->systemTabBarModel()->addSystem(systemDescription, logonData);
    emit currentSystemChanged(systemDescription);
}

void QnWorkbench::addSystem(const QString& systemId, const LogonData& logonData)
{
    auto systemDescription = qnSystemsFinder->getSystem(systemId);

    NX_ASSERT(systemDescription);

    windowContext()->systemTabBarModel()->addSystem(systemDescription, logonData);
    emit currentSystemChanged(systemDescription);
}

void QnWorkbench::setCurrentLayout(QnWorkbenchLayout *layout)
{
    if (!layout)
        layout = m_dummyLayout;

    if (m_currentLayout == layout)
        return;

    QString password;
    if (layout) // layout == nullptr is ok in QnWorkbench destructor
    {
        auto resource = layout->resource().dynamicCast<QnFileLayoutResource>();
        if (resource && resource->requiresPassword())
        {
            // When opening encrypted layout, ask for the password and remove layout if unsuccessful.
            password = layout::askPassword(resource.dynamicCast<QnLayoutResource>(), mainWindowWidget());
            if (password.isEmpty())
            {
                delete layout;
                return;
            }
            layout::reloadFromFile(resource, password);
            if (auto synchronizer = QnWorkbenchLayoutSynchronizer::instance(resource))
                synchronizer->update();

            auto systemContext = SystemContext::fromResource(resource);
            if (NX_ASSERT(systemContext))
                systemContext->layoutSnapshotManager()->store(resource);
        }
    }

    m_inLayoutChangeProcess = true;
    emit layoutChangeProcessStarted();

    if(!m_layouts.contains(layout) && layout != m_dummyLayout)
        addLayout(layout);

    emit currentLayoutAboutToBeChanged();


    /* Clean up old layout.
     * It may be nullptr only when this function is called from constructor. */
    qreal oldCellAspectRatio = -1.0;
    qreal oldCellSpacing = -1.0;
    if (m_currentLayout)
    {
        oldCellAspectRatio = m_currentLayout->cellAspectRatio();
        oldCellSpacing = m_currentLayout->cellSpacing();

        const auto activeItem = m_itemByRole[Qn::ActiveRole];
        m_currentLayout->setData(Qn::LayoutActiveItemRole, activeItem
            ? QVariant::fromValue(activeItem->uuid())
            : QVariant());

        for(int i = 0; i < Qn::ItemRoleCount; i++)
            setItem(static_cast<Qn::ItemRole>(i), nullptr);

        disconnect(m_currentLayout, SIGNAL(itemAdded(QnWorkbenchItem *)),           this, nullptr);
        disconnect(m_currentLayout, SIGNAL(itemRemoved(QnWorkbenchItem *)),         this, nullptr);
        disconnect(m_currentLayout, SIGNAL(cellAspectRatioChanged()),               this, nullptr);
        disconnect(m_currentLayout, SIGNAL(cellSpacingChanged()),                   this, nullptr);
    }

    /* Prepare new layout. */
    m_currentLayout = layout;
    if(m_currentLayout == nullptr && m_dummyLayout != nullptr) {
        m_dummyLayout->clear();
        m_currentLayout = m_dummyLayout;
    }

    /* Set up new layout.
     *
     * The fact that new layout is nullptr means that we're in destructor, so no
     * signals should be emitted. */
    if(m_currentLayout == nullptr)
        return;

    const auto activeItemUuid = m_currentLayout->data(Qn::LayoutActiveItemRole).value<QnUuid>();
    if (!activeItemUuid.isNull())
        setItem(Qn::ActiveRole, m_currentLayout->item(activeItemUuid));

    connect(m_currentLayout,    SIGNAL(itemAdded(QnWorkbenchItem *)),           this, SLOT(at_layout_itemAdded(QnWorkbenchItem *)));
    connect(m_currentLayout,    SIGNAL(itemRemoved(QnWorkbenchItem *)),         this, SLOT(at_layout_itemRemoved(QnWorkbenchItem *)));
    connect(m_currentLayout,    SIGNAL(cellAspectRatioChanged()),               this, SLOT(at_layout_cellAspectRatioChanged()));
    connect(m_currentLayout,    SIGNAL(cellSpacingChanged()),                   this, SLOT(at_layout_cellSpacingChanged()));

    const qreal newCellAspectRatio = m_currentLayout->cellAspectRatio();
    const qreal newCellSpacing = m_currentLayout->cellSpacing();

    if(!qFuzzyEquals(newCellAspectRatio, oldCellAspectRatio))
        at_layout_cellAspectRatioChanged();

    if(!qFuzzyEquals(newCellSpacing, oldCellSpacing))
        at_layout_cellSpacingChanged();

    updateActiveRoleItem();

    m_inLayoutChangeProcess = false;

    emit currentLayoutChanged();
    emit layoutChangeProcessFinished();
}

QnWorkbenchItem *QnWorkbench::item(Qn::ItemRole role) {
    NX_ASSERT(role >= 0 && role < Qn::ItemRoleCount);

    return m_itemByRole[role];
}

void QnWorkbench::setItem(Qn::ItemRole role, QnWorkbenchItem *item)
{
    NX_ASSERT(role >= 0 && role < Qn::ItemRoleCount);
    if (m_itemByRole[role] == item)
        return;

    // Do not set focus at the item if it is not allowed.
    const auto widget = display()->widget(item);
    if (widget
        && !widget->options().testFlag(QnResourceWidget::AllowFocus)
        && role > Qn::SingleSelectedRole)
    {
        return;
    }

    // Delayed clicks may lead to very interesting scenarios. Just ignore.
    bool validLayout = !item || item->layout() == m_currentLayout;
    if (!validLayout)
        return;

    emit itemAboutToBeChanged(role);
    m_itemByRole[role] = item;
    emit itemChanged(role);

    /* If we are changing focus, make sure raised item will be un-raised. */
    if (role > Qn::RaisedRole
        && m_itemByRole[Qn::RaisedRole]
        && m_itemByRole[Qn::RaisedRole] != item
        )
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

void QnWorkbench::updateSingleRoleItem() {
    if(m_currentLayout->items().size() == 1) {
        setItem(Qn::SingleRole, *m_currentLayout->items().begin());
    } else {
        setItem(Qn::SingleRole, nullptr);
    }
}

void QnWorkbench::updateActiveRoleItem(const QnWorkbenchItem* removedItem)
{
    auto activeItem = bestItemForRole(m_itemByRole, Qn::ActiveRole, removedItem);
    if (activeItem)
    {
        setItem(Qn::ActiveRole, activeItem);
        return;
    }

    if (m_itemByRole[Qn::ActiveRole] && m_itemByRole[Qn::ActiveRole] != removedItem)
        return;

    // Try to select the same camera as just removed (if any left).
    QSet<QnWorkbenchItem*> sameResourceItems;
    if (removedItem)
        sameResourceItems = m_currentLayout->items(removedItem->resource());

    const auto& itemsToSelectFrom = sameResourceItems.empty()
        ? m_currentLayout->items()
        : sameResourceItems;

    if (!itemsToSelectFrom.isEmpty())
        activeItem = *itemsToSelectFrom.begin();

    setItem(Qn::ActiveRole, activeItem);
}

void QnWorkbench::updateCentralRoleItem() {
    setItem(Qn::CentralRole, bestItemForRole(m_itemByRole, Qn::CentralRole));
}

void QnWorkbench::update(const QnWorkbenchState& state)
{
    clear();

    for (const auto& id: state.layoutUuids)
    {
        if (const auto layout = resourcePool()->getResourceById<QnLayoutResource>(id))
        {
            const auto factory = LayoutsFactory::instance(this);
            const auto workbenchLayout = factory->create(layout, this);
            addLayout(workbenchLayout);
            continue;
        }

        if (const auto videoWall = resourcePool()->getResourceById<QnVideoWallResource>(id))
        {
            menu()->trigger(action::OpenVideoWallReviewAction, videoWall);
            continue;
        }

        const auto tour = layoutTourManager()->tour(id);
        if (tour.isValid())
        {
            menu()->trigger(action::ReviewLayoutTourAction, {Qn::UuidRole, id});
            continue;
        }
    }

    if (ini().enableMultiSystemTabBar && context()->user())
    {
        const auto userId = context()->user()->getId();
        for (const auto& stateLayout: state.unsavedLayouts)
        {
            if (stateLayout.parentId != userId)
                continue;

            QnResourcePtr resource = resourcePool()->getResourceById(stateLayout.id);
            QnLayoutResourcePtr layoutResource = resource.dynamicCast<QnLayoutResource>();

            if (!layoutResource)
            {
                layoutResource.reset(new QnLayoutResource());
                layoutResource->addFlags(Qn::local);
                layoutResource->setParentId(stateLayout.parentId);
                layoutResource->setIdUnsafe(stateLayout.id);
                resourcePool()->addResource(layoutResource);
            }

            layoutResource->setName(stateLayout.name);
            layoutResource->setCellSpacing(stateLayout.cellSpacing);
            layoutResource->setCellAspectRatio(stateLayout.cellAspectRatio);
            layoutResource->setBackgroundImageFilename(stateLayout.backgroundImageFilename);
            layoutResource->setBackgroundOpacity(stateLayout.backgroundOpacity);
            layoutResource->setBackgroundSize(stateLayout.backgroundSize);

            QnLayoutItemDataList items;
            for (const auto& itemData: stateLayout.items)
            {
                items.append(itemData);
            }
            layoutResource->setItems(items);

            if (QnWorkbenchLayout* layout = findLayout(stateLayout.id))
            {
                layout->update(layoutResource);
            }
            else
            {
                layout = qnWorkbenchLayoutsFactory->create(layoutResource, this);
                addLayout(layout);
            }
        }
    }

    if (!state.currentLayoutId.isNull())
    {
        const auto layout = resourcePool()->getResourceById<QnLayoutResource>(state.currentLayoutId);
        if (layout)
        {
            if (const auto wbLayout = QnWorkbenchLayout::instance(layout))
                setCurrentLayout(wbLayout);
        }
    }

    // Empty workbench is not supported correctly.
    // Create a new layout in the case there are no opened layouts in the workbench state.
    if (layouts().isEmpty())
        menu()->trigger(ui::action::OpenNewTabAction);

    if (currentLayoutIndex() == -1)
        setCurrentLayoutIndex(layouts().size() - 1);

    if (!state.runningTourId.isNull())
    {
        menu()->trigger(action::ToggleLayoutTourModeAction, {Qn::UuidRole, state.runningTourId});
    }
}

void QnWorkbench::submit(QnWorkbenchState& state)
{
    auto isLayoutSupported = [](const QnLayoutResourcePtr& layout)
        {
            // Support layout tours.
            if (!layout->data(Qn::LayoutTourUuidRole).value<QnUuid>().isNull())
                return true;

            if (layout->data().contains(Qn::VideoWallResourceRole))
                return true;

            // Ignore other service layouts, e.g. videowall control layouts.
            if (layout->hasFlags(Qn::local) || layout->isServiceLayout())
                return false;

            return true;
        };

    auto sourceId = [](const QnLayoutResourcePtr& layout)
        {
            if (const auto videoWall = layout->data(Qn::VideoWallResourceRole)
                .value<QnVideoWallResourcePtr>())
            {
                return videoWall->getId();
            }

            const auto tourId = layout->data(Qn::LayoutTourUuidRole).value<QnUuid>();
            return tourId.isNull()
                ? layout->getId()
                : tourId;
        };

    {
        auto currentResource = currentLayout()->resource();
        if (currentResource && isLayoutSupported(currentResource))
            state.currentLayoutId = sourceId(currentResource);
    }

    for (const auto& layout: m_layouts)
    {
        auto resource = layout->resource();
        if (resource)
        {
            if (isLayoutSupported(resource))
                state.layoutUuids.push_back(sourceId(resource));

            auto systemContext = SystemContext::fromResource(resource);
            const bool isSaveable = NX_ASSERT(systemContext)
                && systemContext->layoutSnapshotManager()->isSaveable(resource);

            if (ini().enableMultiSystemTabBar && (resource->hasFlags(Qn::local) || isSaveable))
            {
                QnWorkbenchState::UnsavedLayout unsavedLayout;
                unsavedLayout.id = resource->getId();
                unsavedLayout.parentId = resource->getParentId();
                unsavedLayout.name = layout->name();
                unsavedLayout.cellSpacing = resource->cellSpacing();
                unsavedLayout.cellAspectRatio = resource->cellAspectRatio();
                unsavedLayout.backgroundImageFilename = resource->backgroundImageFilename();
                unsavedLayout.backgroundOpacity = resource->backgroundOpacity();
                unsavedLayout.backgroundSize = resource->backgroundSize();

                for (const auto& itemData: resource->getItems().values())
                {
                    unsavedLayout.items << itemData;
                }
                state.unsavedLayouts << unsavedLayout;
            }
        }
    }

    state.runningTourId = context()->instance<LayoutToursHandler>()->runningTour();
}

void QnWorkbench::applyLoadedState()
{
    m_stateDelegate->updateWorkbench();
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbench::at_layout_itemAdded(QnWorkbenchItem *item) {
    Q_UNUSED(item)
    updateSingleRoleItem();
    emit currentLayoutItemsChanged();
}

void QnWorkbench::at_layout_itemRemoved(QnWorkbenchItem* item)
{
    updateSingleRoleItem();
    updateActiveRoleItem(item);

    for (int i = 0; i < Qn::ItemRoleCount; i++)
    {
        if (item == m_itemByRole[i])
            setItem(static_cast<Qn::ItemRole>(i), nullptr);
    }

    emit currentLayoutItemsChanged();
}

void QnWorkbench::at_layout_aboutToBeDestroyed() {
    removeLayout(checked_cast<QnWorkbenchLayout *>(sender()));
}

void QnWorkbench::at_layout_cellAspectRatioChanged() {
    qreal cellAspectRatio = m_currentLayout->hasCellAspectRatio()
                            ? m_currentLayout->cellAspectRatio()
                            : QnLayoutResource::kDefaultCellAspectRatio;

    m_mapper->setCellSize(kUnitSize, kUnitSize / cellAspectRatio);

    emit cellAspectRatioChanged();
}

void QnWorkbench::at_layout_cellSpacingChanged() {
    m_mapper->setSpacing(m_currentLayout->cellSpacing() * kUnitSize);

    emit cellSpacingChanged();
}

bool QnWorkbench::isInLayoutChangeProcess() const
{
    return m_inLayoutChangeProcess;
}
