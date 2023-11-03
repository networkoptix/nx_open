// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_tab_bar.h"

#include <QtCore/QScopedValueRollback>
#include <QtCore/QVariant>
#include <QtGui/QContextMenuEvent>
#include <QtWidgets/QLayout>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStyle>

#include <client/client_runtime_settings.h>
#include <client/client_settings.h>
#include <common/common_module.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/string.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource/layout_snapshot_manager.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;

namespace {
static const int kMinimumTabSizeWidth = 50;
}

QnLayoutTabBar::QnLayoutTabBar(QWidget* parent):
    QTabBar(parent),
    QnWorkbenchContextAware(parent),
    m_submit(false),
    m_update(false),
    m_midClickedTab(-1)
{
    /* Set up defaults. */
    setMovable(false);
    setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
    setDrawBase(false);
    setTabsClosable(true);
    setElideMode(Qt::ElideRight);
    setTabShape(this, nx::style::TabShape::Rectangular);
    setUsesScrollButtons(false);

    connect(this, &QTabBar::currentChanged, this, &QnLayoutTabBar::at_currentChanged);
    connect(this, &QTabBar::tabMoved, this, &QnLayoutTabBar::at_tabMoved);

    connect(this, &QTabBar::tabCloseRequested, this,
        [this](int index)
        {
            menu()->trigger(action::CloseLayoutAction,
                QnWorkbenchLayoutList() << m_layouts[index]);
        });

    /* Connect to context. */
    at_workbench_layoutsChanged();
    at_workbench_currentLayoutChanged();

    connect(workbench(), &Workbench::layoutsChanged, this,
        &QnLayoutTabBar::at_workbench_layoutsChanged);
    connect(workbench(), &Workbench::currentLayoutChanged, this,
        &QnLayoutTabBar::at_workbench_currentLayoutChanged);

    // Layouts can currently be stored only in the current and cloud layouts contexts.
    connect(appContext()->currentSystemContext()->layoutSnapshotManager(),
        &LayoutSnapshotManager::layoutFlagsChanged,
        this,
        &QnLayoutTabBar::at_snapshotManager_flagsChanged);

    connect(appContext()->cloudLayoutsSystemContext()->layoutSnapshotManager(),
        &LayoutSnapshotManager::layoutFlagsChanged,
        this,
        &QnLayoutTabBar::at_snapshotManager_flagsChanged);

    m_submit = m_update = true;
}

QnLayoutTabBar::~QnLayoutTabBar()
{
    workbench()->disconnect(this);

    m_submit = m_update = false;
    while (count() > 0)
        removeTab(count() - 1);
}

void QnLayoutTabBar::checkInvariants() const
{
    NX_ASSERT(m_layouts.size() == count());

    if (workbench() && m_submit && m_update)
    {
        if (NX_ASSERT((int) workbench()->layouts().size() == m_layouts.size()))
        {
            for (int i = 0; i < m_layouts.size(); ++i)
                NX_ASSERT(workbench()->layouts()[i].get() == m_layouts[i]);
        }

        NX_ASSERT(workbench()->layoutIndex(workbench()->currentLayout()) == currentIndex()
            || workbench()->layoutIndex(workbench()->currentLayout()) == -1);
    }
}

action::ActionScope QnLayoutTabBar::currentScope() const
{
    return action::TitleBarScope;
}

action::Parameters QnLayoutTabBar::currentParameters(action::ActionScope scope) const
{
    if (scope != action::TitleBarScope)
        return action::Parameters();

    QnWorkbenchLayoutList result;
    int currentIndex = this->currentIndex();
    if (currentIndex >= 0 && currentIndex < m_layouts.size())
        result.push_back(m_layouts[currentIndex]);

    return result;
}

void QnLayoutTabBar::submitCurrentLayout()
{
    if (!m_submit)
        return;

    {
        QScopedValueRollback<bool> guard(m_update, false);
        workbench()->setCurrentLayout(currentIndex() == -1 ? nullptr : m_layouts[currentIndex()]);
    }

    checkInvariants();
}

void QnLayoutTabBar::fixGeometry()
{
    /*
     * Our overridden minimumSizeHint method break QTabBar geometry calculations. As a side effect
     * single layout with a short name may be overlapped by scroll buttons. Reason is: internal
     * calculations depend on fact that minimum width is not less than 100px.
     */
    setUsesScrollButtons(count() > 1);
    if (auto parent = parentWidget())
        parent->layout()->activate();
}

QString QnLayoutTabBar::layoutText(QnWorkbenchLayout* layout) const
{
    if (!layout)
        return QString();

    // Escaping the mnemonic is required to correctly display the tab title with '&' symbol.
    QString baseName = nx::utils::escapeMnemonics(layout->name());

    // videowall control mode
    QnUuid videoWallInstanceGuid = layout->data(Qn::VideoWallItemGuidRole).value<QnUuid>();
    if (!videoWallInstanceGuid.isNull())
    {
        QnVideoWallItemIndex idx = resourcePool()->getVideoWallItemByUuid(videoWallInstanceGuid);
        if (!idx.isNull())
            baseName = idx.item().name;
    }

    LayoutResourcePtr resource = layout->resource();
    auto systemContext = SystemContext::fromResource(resource);
    if (!systemContext) //< Perfectly valid for temporary layouts like showreel.
        return baseName;

    // It makes no sense to show locked layout as modified as saving is prohibited anyway.
    if (layout->locked())
        return baseName;

    return systemContext->layoutSnapshotManager()->isModified(resource)
        ? baseName + '*'
        : baseName;
}

QIcon QnLayoutTabBar::layoutIcon(QnWorkbenchLayout* layout) const
{
    if (!layout)
        return QIcon();

    const auto layoutIcon = layout->icon();
    if (!layoutIcon.isNull())
        return layoutIcon;

    // TODO: #ynikitenkov #high refactor code below to use only layout->icon()

    // TODO: #sivanov Refactor this logic as in Alarm Layout. Tab bar should not know layout
    // internal structure.
    if (!layout->data(Qn::VideoWallResourceRole).value<QnVideoWallResourcePtr>().isNull())
        return qnResIconCache->icon(QnResourceIconCache::VideoWall);

    // videowall control mode
    QnUuid videoWallInstanceGuid = layout->data(Qn::VideoWallItemGuidRole).value<QnUuid>();
    if (!videoWallInstanceGuid.isNull())
    {
        QnVideoWallItemIndex idx = resourcePool()->getVideoWallItemByUuid(videoWallInstanceGuid);
        if (idx.isNull())
            return QIcon();

        if (idx.item().runtimeStatus.online)
        {
            if (idx.item().runtimeStatus.controlledBy.isNull())
                return qnResIconCache->icon(QnResourceIconCache::VideoWallItem);
            if (idx.item().runtimeStatus.controlledBy == systemContext()->peerId())
                return qnResIconCache->icon(QnResourceIconCache::VideoWallItem | QnResourceIconCache::Control);
            return qnResIconCache->icon(QnResourceIconCache::VideoWallItem | QnResourceIconCache::Locked);
        }
        return qnResIconCache->icon(QnResourceIconCache::VideoWallItem | QnResourceIconCache::Offline);
    }

    QnLayoutResourcePtr resource = layout->resource();
    if (resource && resource->locked())
        return qnSkin->icon("layouts/locked.png");

    return QIcon();
}

void QnLayoutTabBar::updateTabText(QnWorkbenchLayout *layout)
{
    if (!layout)
        return;

    int idx = m_layouts.indexOf(layout);
    QString oldText = tabText(idx);
    QString newText = layoutText(layout);
    if (oldText == newText)
        return;

    setTabText(idx, newText);
    fixGeometry();
    emit tabTextChanged();
}

void QnLayoutTabBar::updateTabIcon(QnWorkbenchLayout *layout)
{
    setTabIcon(m_layouts.indexOf(layout), layoutIcon(layout));
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
QSize QnLayoutTabBar::minimumSizeHint() const
{
    int d = 2 * style()->pixelMetric(QStyle::PM_TabBarScrollButtonWidth, nullptr, this);
    switch (shape())
    {
        case RoundedNorth:
        case RoundedSouth:
        case TriangularNorth:
        case TriangularSouth:
            return QSize(d, sizeHint().height());
        case RoundedWest:
        case RoundedEast:
        case TriangularWest:
        case TriangularEast:
            return QSize(sizeHint().width(), d);
        default:
            return QSize(); /* Just to make the compiler happy. */
    }
}

QSize QnLayoutTabBar::tabSizeHint(int index) const
{
    auto result = base_type::tabSizeHint(index);
    if (result.width() < kMinimumTabSizeWidth)
        result.setWidth(kMinimumTabSizeWidth);
    return result;
}

QSize QnLayoutTabBar::minimumTabSizeHint(int index) const
{
    auto result = base_type::minimumTabSizeHint(index);
    if (result.width() < kMinimumTabSizeWidth)
        result.setWidth(kMinimumTabSizeWidth);
    return result;
}

void QnLayoutTabBar::contextMenuEvent(QContextMenuEvent *event)
{
    if (qnRuntime->isVideoWallMode())
        return;

    if (!context() || !context()->menu())
    {
        NX_ASSERT(false, "Requesting context menu for a layout tab bar while no menu manager instance is available.");
        return;
    }

    QnWorkbenchLayoutList target;
    int index = tabAt(event->pos());
    if (index >= 0 && index < m_layouts.size())
        target.push_back(m_layouts[index]);

    QScopedPointer<QMenu> menu(context()->menu()->newMenu(
        action::TitleBarScope, mainWindowWidget(), target));
    if (menu->isEmpty())
        return;

    /**
     * Note that we cannot use event->globalPos() here as it doesn't work when
     * the widget is embedded into graphics scene.
     */

    QnHiDpiWorkarounds::showMenu(menu.data(), QCursor::pos());
}

void QnLayoutTabBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton)
    {
        m_midClickedTab = tabAt(event->pos());
    }
    else
    { /* QTabBar ignores event if MiddleButton was clicked. */
        QTabBar::mousePressEvent(event);
    }
    event->accept();
}

void QnLayoutTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MiddleButton)
    {
        if (m_midClickedTab >= 0 && m_midClickedTab == tabAt(event->pos()))
            emit tabCloseRequested(m_midClickedTab);
        m_midClickedTab = -1;
    }
    QTabBar::mouseReleaseEvent(event);
    event->accept();
}

void QnLayoutTabBar::showEvent(QShowEvent* event)
{
    base_type::showEvent(event);
    updateGeometry();
}

void QnLayoutTabBar::at_workbench_layoutsChanged()
{
    if (!m_update)
        return;

    QnWorkbenchLayoutList layouts;
    for (const auto& layout: workbench()->layouts())
        layouts.push_back(layout.get());

    if (m_layouts == layouts)
        return;

    {
        QScopedValueRollback<bool> guard(m_submit, false);

        for (int i = 0; i < layouts.size(); i++)
        {
            int index = m_layouts.indexOf(layouts[i]);
            if (index == -1)
            {
                m_layouts.insert(i, layouts[i]);
                insertTab(i, layoutIcon(layouts[i]), layoutText(layouts[i]));
            }
            else
            {
                moveTab(index, i);
            }
        }

        while (count() > layouts.size())
            removeTab(count() - 1);

        /* Force parent widget layout recalculation: */
        fixGeometry();

        /* Current layout may have changed. Sync. */
        at_workbench_currentLayoutChanged();
    }

    checkInvariants();
}

void QnLayoutTabBar::at_workbench_currentLayoutChanged()
{
    if (!m_update)
        return;

    /* It is important to check indices equality because it will also work
     * for invalid current index (-1). */
    int newCurrentIndex = workbench()->layoutIndex(workbench()->currentLayout());
    if (currentIndex() == newCurrentIndex)
        return;

    {
        QScopedValueRollback<bool> guard(m_submit, false);
        setCurrentIndex(newCurrentIndex);
    }

    checkInvariants();
}

void QnLayoutTabBar::at_snapshotManager_flagsChanged(const QnLayoutResourcePtr &resource)
{
    auto layout = resource.dynamicCast<LayoutResource>();
    updateTabText(workbench()->layout(layout));
}

void QnLayoutTabBar::tabInserted(int index)
{
    {
        QScopedValueRollback<bool> guard(m_update, false);

        QnWorkbenchLayout* layout = m_layouts[index];
        connect(layout,
            &QnWorkbenchLayout::titleChanged,
            this,
            [this, layout]
            {
                updateTabText(layout);
                updateTabIcon(layout);
            });

        // FIXME: #sivanov Investigate if this is needed.
        //if (m_submit)
        //    workbench()->insertLayout(layout, index);
        submitCurrentLayout();
    }

    checkInvariants();
    setMovable(count() > 1);
}

void QnLayoutTabBar::tabRemoved(int index)
{
    {
        QScopedValueRollback<bool> guard(m_update, false);
        m_layouts.removeAt(index);
        submitCurrentLayout();

        if (m_submit)
            workbench()->removeLayout(index);
    }

    checkInvariants();
    setMovable(count() > 1);
}

void QnLayoutTabBar::at_tabMoved(int from, int to)
{
    {
        QScopedValueRollback<bool> guard(m_update, false);

        QnWorkbenchLayout *layout = m_layouts[from];
        m_layouts.move(from, to);

        if (m_submit)
            workbench()->moveLayout(layout, to);
    }

    checkInvariants();
}

void QnLayoutTabBar::at_currentChanged(int index)
{
    Q_UNUSED(index);

    if (m_layouts.size() != count())
        return; /* Was called from add/remove function before our handler was able to sync, current layout will be updated later. */

    submitCurrentLayout();

    checkInvariants();
}

