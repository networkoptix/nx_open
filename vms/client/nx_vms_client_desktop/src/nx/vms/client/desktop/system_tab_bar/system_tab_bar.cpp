// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_tab_bar.h"

#include <QtCore/QMimeData>
#include <QtCore/QScopedValueRollback>
#include <QtGui/QAction>
#include <QtGui/QDrag>
#include <QtGui/QPainter>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMenu>
#include <QtWidgets/QToolButton>

#include <nx/vms/client/core/network/credentials_manager.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/core/system_finder/system_description.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/action_parameters.h>
#include <nx/vms/client/desktop/skin/font_config.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/data/logon_data.h>
#include <nx/vms/client/desktop/system_logon/ui/welcome_screen.h>
#include <nx/vms/client/desktop/window_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <ui/common/indents.h>
#include <ui/widgets/main_window.h>
#include <ui/widgets/main_window_title_bar_state.h>
#include <ui/widgets/system_tab_bar_state_handler.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <utils/common/delayed.h>

#include "private/close_tab_button.h"

namespace {

static const int kFixedTabSizeHeight = 32;
static const int kFixedHomeIconWidth = 32;
static const int kFixedWideHomeIconWidth = 65;
static const QnIndents kTabIndents = {14, 10};
static const char* kDragActionMimeData = "system-tab-move";

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kSystemSubstitutions =
    {{QnIcon::Normal, {.primary = "light4"}}};
static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kAddSubstitutions =
    {{QnIcon::Normal, {.primary = "dark14"}}};

NX_DECLARE_COLORIZED_ICON(kSystemIcon, "20x20/Solid/system_local.svg", kSystemSubstitutions)
NX_DECLARE_COLORIZED_ICON(kAddIcon, "20x20/Outline/add.svg", kAddSubstitutions)

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
    const auto closeButton = new CloseTabButton(this);
    setTabButton(0, RightSide, closeButton);

    connect(closeButton, &CloseTabButton::clicked, this,
        [this]()
        {
            const auto state = m_store->state();

            const auto systemIndex =
                state.activeSystemTab >= 0 && state.activeSystemTab < state.systems.size()
                    ? state.activeSystemTab
                    : (state.systems.size() - 1);

            if (systemIndex >= 0)
            {
                const auto systemData = state.systems[systemIndex];
                if (systemData.systemDescription->localId() == state.currentSystemId)
                    action(menu::ResourcesModeAction)->setChecked(true);
                else
                    connectToSystem(systemData.systemDescription, systemData.logonData);
            }
        });

    updateHomeTabView();
}

void SystemTabBar::connectToSystem(
    const core::SystemDescriptionPtr& system, const LogonData& logonData)
{
    executeLater(
        [this, system, ld = logonData]()
        {
            auto logonData = adjustedLogonData(ld, system->localId());
            if (logonData.credentials.authToken.empty())
            {
                action(menu::ResourcesModeAction)->setChecked(false);
                mainWindow()->welcomeScreen()->openArbitraryTile(system->id());
                m_store->removeSystem(system->localId());
                return;
            }

            menu()->trigger(menu::ConnectAction, menu::Parameters()
                .withArgument(Qn::LogonDataRole, logonData));
        },
        this);
}

LogonData SystemTabBar::adjustedLogonData(const LogonData& source, const nx::Uuid& localId) const
{
    LogonData adjusted = source;

    if (const auto credentials = core::CredentialsManager::credentials(localId);
        !credentials.empty())
    {
        adjusted.credentials = credentials[0];
    }

    adjusted.connectScenario = ConnectScenario::connectFromTabBar;
    adjusted.storePassword = true; //< We don't want to clear a saved password.

    return adjusted;
}

void SystemTabBar::setStateStore(QSharedPointer<Store> store,
    QSharedPointer<StateHandler> stateHandler)
{
    if (m_store)
        m_store->disconnect(this);

    if (m_stateHandler)
        m_stateHandler->disconnect(this);

    m_store = store;
    m_stateHandler = stateHandler;

    connect(m_stateHandler.get(), &StateHandler::tabsChanged, this, &SystemTabBar::rebuildTabs);

    connect(m_stateHandler.get(), &StateHandler::activeSystemTabChanged, this,
        [this](int index)
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
        if (tabData(i).value<core::SystemDescriptionPtr>() == systemData->systemDescription)
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
        action(menu::ResourcesModeAction)->setChecked(false);
    }
    else if (const auto systemData = m_store->systemData(index))
    {
        if (systemData->systemDescription->localId() == m_store->state().currentSystemId)
            action(menu::ResourcesModeAction)->setChecked(true);
        else
            connectToSystem(systemData->systemDescription, systemData->logonData);
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

void SystemTabBar::contextMenuEvent(QContextMenuEvent* event)
{
    const int index = tabAt(event->pos());
    if (index == homeTabIndex())
        return;

    QMenu menu(this);
    menu.addAction(tr("Disconnect"),
        [this, index]()
        {
            if (const auto systemData = m_store->systemData(index))
                disconnectFromSystem(systemData->systemDescription->localId());
        });

    menu.addAction(tr("Open in New Window"),
        [this, index]()
        {
            if (const auto systemData = m_store->systemData(index))
            {
                const auto logonData = adjustedLogonData(systemData->logonData,
                    systemData->systemDescription->localId());

                if (logonData.credentials.authToken.empty())
                {
                    // TODO: VMS-54449: Request missing credentials.
                }

                appContext()->clientStateHandler()->createNewWindow(logonData);
            }
        });

    QnHiDpiWorkarounds::showMenu(&menu, QCursor::pos());
}

bool SystemTabBar::disconnectFromSystem(const nx::Uuid& localId)
{
    if (!windowContext()->connectActionsHandler()->askDisconnectConfirmation())
        return false;

    const auto state = m_store->state();
    if (state.currentSystemId == localId)
    {
        if (state.systems.size() == 1)
            action(menu::ResourcesModeAction)->setChecked(false);

        if (isHomeTabActive())
        {
            action(menu::DisconnectAction)->trigger();
        }
        else
        {
            const int systemIndex = state.activeSystemTab == state.systems.size() - 1
                ? (state.activeSystemTab - 1)
                : (state.activeSystemTab + 1);

            const auto systemData = state.systems[systemIndex];
            connectToSystem(systemData.systemDescription, systemData.logonData);
        }
    }

    return true;
}

void SystemTabBar::insertClosableTab(int index,
    const QString& text,
    const core::SystemDescriptionPtr& systemDescription)
{
    insertTab(index, text);
    setTabData(index, QVariant::fromValue(systemDescription));

    const auto closeButton = new CloseTabButton(this);
    connect(closeButton, &CloseTabButton::clicked,
        [this, localId = systemDescription->localId()]()
        {
            if (disconnectFromSystem(localId))
                m_store->removeSystem(localId);
        });

    // closeButton is owned by the tab, so we can use it as a receiver here.
    // This connection is deleted when the tab is removed.
    connect(systemDescription.get(), &core::SystemDescription::systemNameChanged, closeButton,
        [this, systemDescription]()
        {
            const auto data = QVariant::fromValue(systemDescription);
            for (int i = 0; i < count(); ++i)
            {
                if (tabData(i) == data)
                {
                    setTabText(i, systemDescription->name());
                    break;
                }
            }
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
        setTabIcon(index, qnSkin->icon(kSystemIcon));
    }
    else if (isHomeTabActive())
    {
        // Home tab is active.
        closeButton->setVisible(true);
        setTabIcon(index, qnSkin->icon(kSystemIcon));
    }
    else
    {
        // Home tab is inactive.
        closeButton->setVisible(false);
        setTabIcon(index, qnSkin->icon(kAddIcon));
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
