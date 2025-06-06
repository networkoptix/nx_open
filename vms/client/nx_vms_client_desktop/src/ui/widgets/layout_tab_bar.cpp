// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_tab_bar.h"

#include <QtCore/QScopedValueRollback>
#include <QtCore/QVariant>
#include <QtGui/QContextMenuEvent>
#include <QtWidgets/QLayout>
#include <QtWidgets/QMenu>
#include <QtWidgets/QStyle>

#include <client/client_runtime_settings.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_item_index.h>
#include <core/resource/videowall_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/string.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/resource/layout_resource.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/skin/font_config.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/common/indents.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>

using namespace nx::vms::client;
using namespace nx::vms::client::desktop;

using nx::vms::client::core::LayoutResource;
using nx::vms::client::core::LayoutResourcePtr;

namespace {

static constexpr int kMinimumTabWidth = 64;
static constexpr int kMaximumTabWidth = 196;
static const QnIndents kTabIndents = {12, 8};
static const QMargins kSingleLevelContentMargins = {1, 0, 1, 0};
static const QMargins kDoubleLevelContentMargins = {0, 4, 1, 4};

// Palette colors for tabs in different modes:
// - QPalette::Text - text;
// - QPalette::Base - background;
// - QPalette::Dark - right margin;
// - QPalette::Light - left margin.

QPalette singleLevelPalette(QWidget* widget)
{
    auto p = widget->palette();
    // Active tab:
    p.setColor(QPalette::Active, QPalette::Text, core::colorTheme()->color("light4"));
    p.setColor(QPalette::Active, QPalette::Base, core::colorTheme()->color("dark10"));
    p.setColor(QPalette::Active, QPalette::Light, core::colorTheme()->color("dark4"));
    p.setColor(QPalette::Active, QPalette::Dark, core::colorTheme()->color("dark4"));

    // Inactive tab:
    p.setColor(QPalette::Inactive, QPalette::Text, core::colorTheme()->color("light16"));
    p.setColor(QPalette::Inactive, QPalette::Base, core::colorTheme()->color("dark7"));
    p.setColor(QPalette::Inactive, QPalette::Light, core::colorTheme()->color("dark6"));
    p.setColor(QPalette::Inactive, QPalette::Dark, core::colorTheme()->color("dark8"));
    return p;
}

QPalette doubleLevelPalette(QWidget* widget)
{
    auto p = widget->palette();
    // Active tab:
    p.setColor(QPalette::Active, QPalette::Text, core::colorTheme()->color("light4"));
    p.setColor(QPalette::Active, QPalette::Base, core::colorTheme()->color("dark10"));

    // Inactive tab:
    p.setColor(QPalette::Inactive, QPalette::Text, core::colorTheme()->color("light10"));
    p.setColor(QPalette::Inactive, QPalette::Base, core::colorTheme()->color("dark10"));

    // Common
    p.setColor(QPalette::Dark, core::colorTheme()->color("dark12"));
    return p;
}

} // namespace

QnLayoutTabBar::QnLayoutTabBar(
    nx::vms::client::desktop::WindowContext* windowContext,
    QWidget* parent)
    :
    QTabBar(parent),
    WindowContextAware(windowContext),
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
    setProperty(nx::style::Properties::kSideIndentation, QVariant::fromValue(kTabIndents));

    if (ini().enableMultiSystemTabBar)
    {
        setDoubleLevelContentMargins();
        setDoubleLevelPalette();
    }
    else
    {
        setSingleLevelContentMargins();
        setSingleLevelPalette();
    }

    setFont(fontConfig()->font("systemTabBar"));
    setProperty(nx::style::Properties::kDontPolishFontProperty, true);

    connect(this, &QTabBar::currentChanged, this, &QnLayoutTabBar::at_currentChanged);
    connect(this, &QTabBar::tabMoved, this, &QnLayoutTabBar::at_tabMoved);

    connect(this, &QTabBar::tabCloseRequested, this,
        [this](int index)
        {
            menu()->trigger(menu::CloseLayoutAction,
                QnWorkbenchLayoutList() << m_layouts[index]);
        });

    /* Connect to context. */
    at_workbench_layoutsChanged();
    at_workbench_currentLayoutChanged();

    connect(workbench(), &Workbench::layoutsChanged, this,
        &QnLayoutTabBar::at_workbench_layoutsChanged);
    connect(workbench(), &Workbench::currentLayoutChanged, this,
        &QnLayoutTabBar::at_workbench_currentLayoutChanged);

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

menu::ActionScope QnLayoutTabBar::currentScope() const
{
    return menu::TitleBarScope;
}

menu::Parameters QnLayoutTabBar::currentParameters(menu::ActionScope scope) const
{
    if (scope != menu::TitleBarScope)
        return menu::Parameters();

    QnWorkbenchLayoutList result;
    int currentIndex = this->currentIndex();
    if (currentIndex >= 0 && currentIndex < m_layouts.size())
        result.push_back(m_layouts[currentIndex]);

    return result;
}

void QnLayoutTabBar::setSingleLevelContentMargins()
{
    setContentsMargins(kSingleLevelContentMargins);
}

void QnLayoutTabBar::setSingleLevelPalette()
{
    setPalette(singleLevelPalette(this));
}

void QnLayoutTabBar::setDoubleLevelContentMargins()
{
    setContentsMargins(kDoubleLevelContentMargins);
}

void QnLayoutTabBar::setDoubleLevelPalette()
{
    setPalette(doubleLevelPalette(this));
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
    nx::Uuid videoWallInstanceGuid = layout->data(Qn::VideoWallItemGuidRole).value<nx::Uuid>();
    if (!videoWallInstanceGuid.isNull())
    {
        QnVideoWallItemIndex idx =
            system()->resourcePool()->getVideoWallItemByUuid(videoWallInstanceGuid);
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

    return resource->canBeSaved() ? (baseName + '*') : baseName;
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
    nx::Uuid videoWallInstanceGuid = layout->data(Qn::VideoWallItemGuidRole).value<nx::Uuid>();
    if (!videoWallInstanceGuid.isNull())
    {
        QnVideoWallItemIndex idx = system()->resourcePool()->getVideoWallItemByUuid(videoWallInstanceGuid);
        if (idx.isNull())
            return QIcon();

        if (idx.item().runtimeStatus.online)
        {
            if (idx.item().runtimeStatus.controlledBy.isNull())
                return qnResIconCache->icon(QnResourceIconCache::VideoWallItem);
            if (idx.item().runtimeStatus.controlledBy == system()->peerId())
                return qnResIconCache->icon(QnResourceIconCache::VideoWallItem | QnResourceIconCache::Control);
            return qnResIconCache->icon(QnResourceIconCache::VideoWallItem | QnResourceIconCache::Locked);
        }
        return qnResIconCache->icon(QnResourceIconCache::VideoWallItem | QnResourceIconCache::Offline);
    }

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
    setTabToolTip(idx, newText);
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
    const auto result = base_type::tabSizeHint(index);
    const int width = std::max(kMinimumTabWidth,
        std::min(result.width() + nx::style::Metrics::kInterSpace, kMaximumTabWidth));
    return QSize(width, result.height());
}

QSize QnLayoutTabBar::minimumTabSizeHint(int index) const
{
    auto result = base_type::minimumTabSizeHint(index);
    if (result.width() < kMinimumTabWidth)
        result.setWidth(kMinimumTabWidth);
    return result;
}

void QnLayoutTabBar::contextMenuEvent(QContextMenuEvent *event)
{
    if (qnRuntime->isVideoWallMode())
        return;

    QnWorkbenchLayoutList target;
    int index = tabAt(event->pos());
    if (index >= 0 && index < m_layouts.size())
        target.push_back(m_layouts[index]);

    QScopedPointer<QMenu> menu(this->menu()->newMenu(
        menu::TitleBarScope, mainWindowWidget(), target));
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

    QList<QPointer<QnWorkbenchLayout>> layouts;
    for (const auto& layout: workbench()->layouts())
        layouts.push_back(layout.get());

    if (layouts == m_layouts)
        return;

    for (const auto& layout: m_layouts)
    {
        if (!layout)
            continue;

        layout->disconnect(this);
        layout->resource()->disconnect(this);
    }

    for (const auto& layout: layouts)
    {
        connect(layout, &QnWorkbenchLayout::titleChanged, this,
            [this, layout]()
            {
                updateTabText(layout);
                updateTabIcon(layout);
            });

        connect(layout->resource().objectCast<LayoutResource>().get(),
            &LayoutResource::canBeSavedChanged,
            this,
            [this, layout = QPointer(layout)]()
            {
                if (layout)
                    updateTabText(layout);
            });
    }

    {
        QScopedValueRollback<bool> guard(m_submit, false);

        for (int i = 0; i < layouts.size(); i++)
        {
            int index = m_layouts.indexOf(layouts[i]);
            if (index == -1)
            {
                m_layouts.insert(i, layouts[i]);
                const auto text = layoutText(layouts[i]);
                insertTab(i, layoutIcon(layouts[i]), text);
                setTabToolTip(i, text);
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

void QnLayoutTabBar::tabInserted(int index)
{
    {
        QScopedValueRollback<bool> guard(m_update, false);

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
