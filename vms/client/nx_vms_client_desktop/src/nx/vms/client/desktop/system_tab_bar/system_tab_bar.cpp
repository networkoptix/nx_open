// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_tab_bar.h"

#include <QtCore/QMimeData>
#include <QtCore/QScopedValueRollback>
#include <QtGui/QAction>
#include <QtGui/QDrag>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QToolButton>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/skin/font_config.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/data/logon_data.h>
#include <nx/vms/client/desktop/system_logon/ui/welcome_screen.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/common/indents.h>
#include <ui/widgets/main_window_title_bar_state.h>
#include <ui/widgets/system_tab_bar_state_handler.h>
#include <utils/common/delayed.h>

#include "private/close_tab_button.h"

namespace {
static const int kFixedTabSizeHeight = 32;
static const int kFixedHomeIconWidth = 32;
static const int kFixedWideHomeIconWidth = 65;
static const QnIndents kTabIndents = {14, 10};
static const char* kDragActionMimeData = "system-tab-move";
} // namespace

namespace nx::vms::client::desktop {

SystemTabBar::SystemTabBar(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    setAcceptDrops(true);
    setSelectionBehaviorOnRemove(QTabBar::SelectLeftTab);
    setDrawBase(false);
    setTabsClosable(false);
    setElideMode(Qt::ElideRight);
    setTabShape(this, nx::style::TabShape::Rectangular);
    setUsesScrollButtons(false);
    setFixedHeight(kFixedTabSizeHeight);
    setContentsMargins(1, 0, 1, 0);
    setProperty(style::Properties::kSideIndentation, QVariant::fromValue(kTabIndents));

    // Set palette colors for tabs:
    // - QPalette::Text - text;
    // - QPalette::Base - background;
    // - QPalette::Dark - right margin;
    // - QPalette::Light - left margin.
    auto p = palette();

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
    setPalette(p);

    // Home tab
    addTab("");
    auto closeButton = new CloseTabButton(this);
    setTabButton(0, RightSide, closeButton);
    connect(closeButton, &CloseTabButton::clicked, this,
        [this]()
        {
            if (m_store)
                m_store->setHomeTabActive(false);
        });
    updateHomeTabView();
}

void SystemTabBar::setStateStore(QSharedPointer<Store> store,
    QSharedPointer<StateHandler> stateHandler)
{
    disconnect(m_store.get());
    m_store = store;
    m_stateHandler = stateHandler;
    connect(m_stateHandler.get(), &StateHandler::tabsChanged, this, &SystemTabBar::rebuildTabs);
    connect(m_stateHandler.get(), &StateHandler::activeSystemTabChanged, this, [this](int index)
        {
            setCurrentIndex(index);
            updateHomeTabView();
        });
}

void SystemTabBar::rebuildTabs()
{
    for (int i = 0; i < m_store->systemCount(); ++i)
    {
        const auto systemData = m_store->systemData(i);
        if (tabData(i).value<QnSystemDescriptionPtr>() == systemData->systemDescription)
            continue;

        if (i < homeTabIndex())
            removeTab(i);

        const auto text = systemData->name();
        insertClosableTab(i, text, systemData->systemDescription);
    }

    while (count() - 1 > m_store->systemCount())
        removeTab(homeTabIndex() - 1);
    updateHomeTabView();
}

void SystemTabBar::activateHomeTab()
{
    setCurrentIndex(homeTabIndex());
}

bool SystemTabBar::isHomeTabActive() const
{
    return isHomeTab(currentIndex());
}

bool SystemTabBar::isUpdating() const
{
    return m_updating;
}

QSize SystemTabBar::tabSizeHint(int index) const
{
    int width;
    if (isHomeTab(index))
    {
        if (count() == 1 || !isHomeTabActive())
            width = kFixedHomeIconWidth;
        else
            width = kFixedWideHomeIconWidth;
    }
    else
    {
        auto size = base_type::tabSizeHint(index);
        width = size.width() + style::Metrics::kStandardPadding;
        if (width < kFixedHomeIconWidth)
            width = kFixedHomeIconWidth;
    }
    return QSize(width, kFixedTabSizeHeight);
}

void SystemTabBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;

    m_dragStartPosition = event->pos();
    const int index = tabAt(event->pos());
    if (isHomeTab(index))
    {
        m_store->setHomeTabActive(true);
    }
    else
    {
        m_store->setHomeTabActive(false);
        if (m_store->state().activeSystemTab != index && index >= 0)
            m_store->setActiveSystemTab(index);
    }
}

void SystemTabBar::mouseMoveEvent(QMouseEvent* event)
{
    if (!(event->buttons() & Qt::LeftButton)
        || isHomeTab(tabAt(m_dragStartPosition))
        || count() <= 2)
    {
        return;
    }

    if ((event->pos() - m_dragStartPosition).manhattanLength() < QApplication::startDragDistance())
        return;

    const int tabIndex = tabAt(m_dragStartPosition);
    if (tabIndex < 0 || tabIndex >= m_store->systemCount())
        return;

    auto drag = new QDrag(this);
    auto mimeData = new QMimeData();
    mimeData->setData("action", kDragActionMimeData);
    mimeData->setData("index", QString::number(tabIndex).toLatin1());
    drag->setPixmap(imageData(tabIndex));
    drag->setMimeData(mimeData);

    auto size = tabSizeHint(tabIndex);
    drag->setHotSpot({size.width() / 2, size.height() * 4 / 5});
    drag->exec(Qt::MoveAction);
}

void SystemTabBar::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->data("action") == kDragActionMimeData)
        event->acceptProposedAction();
}
void SystemTabBar::dropEvent(QDropEvent* event)
{
    const auto mimeData = event->mimeData();
    if (mimeData->data("action") != kDragActionMimeData)
        return;

    bool ok;
    const int indexFrom = QString(mimeData->data("index")).toInt(&ok);
    if (!ok)
        return;

    const int indexTo = tabAt(event->position().toPoint());
    if (indexTo < 0 || indexTo >= homeTabIndex())
        return;

    event->acceptProposedAction();
    m_store->moveSystem(indexFrom, indexTo);
}

void SystemTabBar::insertClosableTab(int index,
    const QString& text,
    const QnSystemDescriptionPtr& systemDescription)
{
    insertTab(index, text);
    setTabData(index, QVariant::fromValue(systemDescription));

    QAbstractButton* closeButton = new CloseTabButton(this);
    connect(closeButton, &CloseTabButton::clicked,
        [this, systemId = systemDescription->localId()]()
        {
            m_store->removeSystem(systemId);
        });
    setTabButton(index, RightSide, closeButton);
}

bool SystemTabBar::isHomeTab(int index) const
{
    return index == homeTabIndex();
}

int SystemTabBar::homeTabIndex() const
{
    return count() - 1;
}

void SystemTabBar::updateHomeTabView()
{
    const int index = homeTabIndex();
    auto closeButton = tabButton(index, RightSide);
    if (count() == 1)
    {
        // Only home tab exists.
        closeButton->setVisible(false);
        setTabIcon(index, qnSkin->icon("titlebar/systems.svg"));
    }
    else if (isHomeTabActive())
    {
        // Home tab is active.
        closeButton->setVisible(true);
        setTabIcon(index, qnSkin->icon("titlebar/systems.svg"));
    }
    else
    {
        // Home tab is inactive.
        closeButton->setVisible(false);
        setTabIcon(index, qnSkin->icon("titlebar/add.svg"));
    }
    resizeEvent({});
    updateGeometry();
}

QPixmap SystemTabBar::imageData(int tabIndex) const
{
    const auto systemData = m_store->systemData(tabIndex);
    if (systemData == std::nullopt)
        return {};

    QSize imageSize = tabSizeHint(tabIndex);
    const QRect rect({0,0}, imageSize);
    imageSize *= devicePixelRatio();

    QPixmap result(imageSize);
    result.setDevicePixelRatio(devicePixelRatio());
    QPainter painter(&result);

    painter.fillRect(rect, palette().color(QPalette::Active, QPalette::Base));
    painter.setPen(palette().color(QPalette::Active, QPalette::Text));
    painter.setFont(fontConfig()->font("systemTabBar"));
    painter.drawText(rect, systemData->name(), {Qt::AlignCenter});

    return result;
}

} // namespace nx::vms::client::desktop
