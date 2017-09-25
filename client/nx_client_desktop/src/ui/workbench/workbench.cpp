#include "workbench.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/layout_tour_manager.h>

#include <core/resource/layout_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>

#include <ui/style/globals.h>

#include <ui/style/skin.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_grid_mapper.h>
#include <ui/workbench/workbench_item.h>

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include <utils/common/util.h>

#include <nx/client/desktop/ui/workbench/layouts/layout_factory.h>
#include <nx/client/desktop/ui/workbench/layouts/special_layout.h>

#include <common/common_module.h>

using namespace nx::client::desktop::ui;
using namespace nx::client::desktop::ui::workbench;

namespace {

QnWorkbenchItem *bestItemForRole(QnWorkbenchItem **itemByRole, Qn::ItemRole role) {
    for(int i = 0; i != role ; i++)
        if(itemByRole[i])
            return itemByRole[i];
    return NULL;
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
    m_currentLayout(NULL),
    m_inLayoutChangeProcess(false)
{
    for(int i = 0; i < Qn::ItemRoleCount; i++)
        m_itemByRole[i] = NULL;

    const auto factory = LayoutsFactory::instance(this);
    addLayoutCreators(factory);
    m_mapper = new QnWorkbenchGridMapper(this);
    m_dummyLayout = factory->create(this);
    setCurrentLayout(m_dummyLayout);
}

QnWorkbench::~QnWorkbench() {
    bool signalsBlocked = blockSignals(false);
    emit aboutToBeDestroyed();
    blockSignals(signalsBlocked);

    m_dummyLayout = NULL;
    clear();
}

void QnWorkbench::clear()
{
    setCurrentLayout(nullptr);

    blockSignals(true);

    for (const auto layout: m_layouts)
        removeLayout(layout);

    blockSignals(false);

    m_layouts.clear();

    emit layoutsChanged();
}

QnWorkbenchLayout *QnWorkbench::layout(int index) const {
    if(index < 0 || index >= m_layouts.size()) {
        qnWarning("Invalid layout index '%1'.", index);
        return NULL;
    }

    return m_layouts[index];
}

void QnWorkbench::addLayout(QnWorkbenchLayout *layout) {
    insertLayout(layout, m_layouts.size());
}

void QnWorkbench::insertLayout(QnWorkbenchLayout *layout, int index) {
    if(layout == NULL) {
        qnNullWarning(layout);
        return;
    }

    if(m_layouts.contains(layout)) {
        qnWarning("Given layout is already in this workbench's layouts list.");
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
    if (!layout)
    {
        qnNullWarning(layout);
        return;
    }

    if (!m_layouts.contains(layout))
        return; /* Removing a layout that is not there is OK. */

    /* Update current layout if it's being removed. */
    if (layout == m_currentLayout)
    {
        QnWorkbenchLayout *newCurrentLayout = NULL;

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
    if(layout == NULL) {
        qnNullWarning(layout);
        return;
    }

    if(!m_layouts.contains(layout)) {
        qnWarning("Given layout is not in this workbench's layouts list.");
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

void QnWorkbench::setCurrentLayout(QnWorkbenchLayout *layout) {
    if(layout == NULL)
        layout = m_dummyLayout;

    if(m_currentLayout == layout)
        return;

    m_inLayoutChangeProcess = true;
    emit layoutChangeProcessStarted();

    if(!m_layouts.contains(layout) && layout != m_dummyLayout)
        addLayout(layout);

    emit currentLayoutAboutToBeChanged();


    /* Clean up old layout.
     * It may be NULL only when this function is called from constructor. */
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
            setItem(static_cast<Qn::ItemRole>(i), NULL);

        disconnect(m_currentLayout, SIGNAL(itemAdded(QnWorkbenchItem *)),           this, NULL);
        disconnect(m_currentLayout, SIGNAL(itemRemoved(QnWorkbenchItem *)),         this, NULL);
        disconnect(m_currentLayout, SIGNAL(cellAspectRatioChanged()),               this, NULL);
        disconnect(m_currentLayout, SIGNAL(cellSpacingChanged()),                   this, NULL);
    }

    /* Prepare new layout. */
    m_currentLayout = layout;
    if(m_currentLayout == NULL && m_dummyLayout != NULL) {
        m_dummyLayout->clear();
        m_currentLayout = m_dummyLayout;
    }

    /* Set up new layout.
     *
     * The fact that new layout is NULL means that we're in destructor, so no
     * signals should be emitted. */
    if(m_currentLayout == NULL)
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
        setItem(Qn::SingleRole, NULL);
    }
}

void QnWorkbench::updateActiveRoleItem() {
    QnWorkbenchItem *activeItem = bestItemForRole(m_itemByRole, Qn::ActiveRole);
    if(activeItem) {
        setItem(Qn::ActiveRole, activeItem);
        return;
    }

    if(m_itemByRole[Qn::ActiveRole])
        return;

    setItem(Qn::ActiveRole, m_currentLayout->items().isEmpty() ? NULL : *m_currentLayout->items().begin());
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

    if (!state.currentLayoutId.isNull())
    {
        const auto layout = resourcePool()->getResourceById<QnLayoutResource>(state.currentLayoutId);
        if (layout)
        {
            if (const auto wbLayout = QnWorkbenchLayout::instance(layout))
                setCurrentLayout(wbLayout);
        }
    }

    if (currentLayoutIndex() == -1 && !layouts().isEmpty())
        setCurrentLayoutIndex(layouts().size() - 1);
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

    for (auto layout: m_layouts)
    {
        auto resource = layout->resource();
        if (resource && isLayoutSupported(resource))
            state.layoutUuids.push_back(sourceId(resource));
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbench::at_layout_itemAdded(QnWorkbenchItem *item) {
    Q_UNUSED(item)
    updateSingleRoleItem();
}

void QnWorkbench::at_layout_itemRemoved(QnWorkbenchItem *item) {
    for(int i = 0; i < Qn::ItemRoleCount; i++)
        if(item == m_itemByRole[i])
            setItem(static_cast<Qn::ItemRole>(i), NULL);

    updateSingleRoleItem();
    updateActiveRoleItem();
}

void QnWorkbench::at_layout_aboutToBeDestroyed() {
    removeLayout(checked_cast<QnWorkbenchLayout *>(sender()));
}

void QnWorkbench::at_layout_cellAspectRatioChanged() {
    qreal unit = qnGlobals->workbenchUnitSize();
    qreal cellAspectRatio = m_currentLayout->hasCellAspectRatio()
                            ? m_currentLayout->cellAspectRatio()
                            : qnGlobals->defaultLayoutCellAspectRatio();

    m_mapper->setCellSize(unit, unit / cellAspectRatio);

    emit cellAspectRatioChanged();
}

void QnWorkbench::at_layout_cellSpacingChanged() {
    qreal unit = qnGlobals->workbenchUnitSize();

    m_mapper->setSpacing(m_currentLayout->cellSpacing() * unit);

    emit cellSpacingChanged();
}

bool QnWorkbench::isInLayoutChangeProcess() const
{
    return m_inLayoutChangeProcess;
}
