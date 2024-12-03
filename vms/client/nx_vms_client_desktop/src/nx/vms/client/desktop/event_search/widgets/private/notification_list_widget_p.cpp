// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "notification_list_widget_p.h"

#include <QtCore/QSortFilterProxyModel>
#include <QtGui/QAction>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QMenu>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

#include <business/business_resource_validation.h>
#include <client/client_globals.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <health/system_health_strings_helper.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/models/subset_list_model.h>
#include <nx/vms/client/desktop/common/utils/custom_painted.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/common/widgets/selectable_text_button.h>
#include <nx/vms/client/desktop/event_search/dialogs/notification_settings_dialog.h>
#include <nx/vms/client/desktop/event_search/models/notification_tab_model.h>
#include <nx/vms/client/desktop/event_search/widgets/abstract_search_widget.h>
#include <nx/vms/client/desktop/event_search/widgets/event_ribbon.h>
#include <nx/vms/client/desktop/event_search/widgets/event_tile.h>
#include <nx/vms/client/desktop/event_search/widgets/placeholder_widget.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/context_utils.h>
#include <nx/vms/client/desktop/utils/user_notification_settings_manager.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <nx/vms/common/saas/saas_utils.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/rules/ini.h>
#include <ui/common/palette.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/event_processors.h>

#include "tile_interaction_handler_p.h"

namespace {

static const nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {.primary = "light16"}},
    {QIcon::Active, {.primary = "light14"}},
    {QIcon::Selected, {.primary = "light10"}},
    {QnIcon::Pressed, {.primary = "light14"}},
};

NX_DECLARE_COLORIZED_ICON(kSystemIcon, "20x20/Outline/system.svg", kIconSubstitutions)

nx::vms::client::core::SvgIconColorer::ThemeSubstitutions kPlaceholderTheme = {
    {QnIcon::Normal, {.primary = "dark10"}}};

NX_DECLARE_COLORIZED_ICON(
    kNotificationPlaceholderIcon, "64x64/Outline/notification.svg", kPlaceholderTheme)

} // namespace

namespace nx::vms::client::desktop {

NotificationListWidget::Private::Private(NotificationListWidget* q):
    QObject(),
    WindowContextAware(utils::windowContextFromObject(q)),
    q(q),
    m_mainLayout(new QVBoxLayout(q)),
    m_headerWidget(new QWidget(q)),
    m_separatorLine(new QFrame(q)),
    m_itemCounterLabel(new QLabel(q)),
    m_ribbonContainer(new QWidget(q)),
    m_filterSystemsButton(new SelectableTextButton(q)),
    m_filterNotificationsButton(new SelectableTextButton(q)),
    m_eventRibbon(new EventRibbon(m_ribbonContainer)),
    m_placeholder(PlaceholderWidget::create(
        qnSkin->icon(kNotificationPlaceholderIcon).pixmap(64, 64),
        AbstractSearchWidget::makePlaceholderText(tr("No new notifications"), {}),
        m_ribbonContainer)),
    m_notificationSettingsDialog{new NotificationSettingsDialog{q}},
    m_model(new NotificationTabModel(windowContext(), this)),
    m_filterModel(new QSortFilterProxyModel(q)),
    m_stringHelper{new event::StringsHelper{system()}}
{
    m_headerWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_headerWidget->setStyleSheet(
        nx::format("QWidget { background-color: %1; border-left: 1px solid %2; }",
            core::colorTheme()->color("dark4").name(),
            core::colorTheme()->color("dark8").name()));

    m_ribbonContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    m_separatorLine->setFrameShape(QFrame::HLine);
    m_separatorLine->setFrameShadow(QFrame::Plain);
    m_separatorLine->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_separatorLine->setMinimumSize({0, 1});
    setPaletteColor(m_separatorLine, QPalette::Shadow, core::colorTheme()->color("dark6"));

    m_mainLayout->setContentsMargins({});
    m_mainLayout->setSpacing(0);
    m_mainLayout->addWidget(m_headerWidget);
    m_mainLayout->addWidget(m_separatorLine);
    m_mainLayout->addWidget(m_ribbonContainer);

    auto headerLayout = new QVBoxLayout(m_headerWidget);
    headerLayout->setContentsMargins(8, 8, 8, 8);
    headerLayout->setSpacing(0);
    headerLayout->addWidget(m_filterSystemsButton);
    setupFilterSystemsButton();

    headerLayout->addWidget(m_filterNotificationsButton);
    setupFilterNotificationsButton();

    anchorWidgetToParent(m_placeholder);
    anchorWidgetToParent(m_eventRibbon);

    const auto viewportHeader = new QWidget(q);
    viewportHeader->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    const auto viewportLayout = new QVBoxLayout(viewportHeader);
    viewportLayout->setContentsMargins(0, 8, 0, 8);
    viewportLayout->addWidget(m_itemCounterLabel);

    m_eventRibbon->setViewportHeader(viewportHeader);
    m_eventRibbon->setViewportMargins(0, 0);

    setPaletteColor(m_itemCounterLabel, QPalette::Text, core::colorTheme()->color("dark16"));
    m_itemCounterLabel->setText({});

    m_eventRibbon->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_eventRibbon->scrollBar()->ensurePolished();
    setPaletteColor(m_eventRibbon->scrollBar(), QPalette::Disabled, QPalette::Midlight,
        core::colorTheme()->color("dark5"));

    m_filterModel->setSourceModel(m_model);
    m_filterModel->setFilterRole(Qn::CloudSystemIdRole);
    auto listFilterModelProxy = new SubsetListModel(m_filterModel, 0, QModelIndex(), q);
    m_eventRibbon->setModel(listFilterModelProxy);

    connect(m_eventRibbon, &EventRibbon::hovered, q, &NotificationListWidget::tileHovered);

    connect(m_eventRibbon,
        &EventRibbon::unreadCountChanged,
        q,
        &NotificationListWidget::unreadCountChanged);

    const auto onCountChanged =
        [this]
        {
            const auto count = m_eventRibbon->count();

            m_itemCounterLabel->setText(count > 0 ? tr("%n notifications", "", count) : "");
            m_placeholder->setVisible(count <= 0);
        };

    connect(m_eventRibbon, &EventRibbon::countChanged, this, onCountChanged);

    connect(workbenchContext(), &QnWorkbenchContext::userChanged, this,
        [this] { changeHeaderItemsVisibilityIfNeeded(); });

    NX_ASSERT(qnCloudStatusWatcher, "Cloud status watcher is not ready");
    connect(qnCloudStatusWatcher,
        &nx::vms::client::core::CloudStatusWatcher::cloudSystemsChanged,
        this,
        [this] { changeHeaderItemsVisibilityIfNeeded(); });

    connect(
        system()->userNotificationSettingsManager(),
        &UserNotificationSettingsManager::settingsChanged,
        this,
        &NotificationListWidget::Private::updateFilterNotificationsButtonAppearance);

    TileInteractionHandler::install(m_eventRibbon);

    onCountChanged();
    changeHeaderItemsVisibilityIfNeeded();
}

NotificationListWidget::Private::~Private()
{
}

void NotificationListWidget::Private::setupFilterSystemsButton()
{
    m_filterSystemsButton->setFlat(true);
    m_filterSystemsButton->setSelectable(false);
    m_filterSystemsButton->setDeactivatable(true);
    m_filterSystemsButton->setIcon(qnSkin->icon(kSystemIcon));
    m_filterSystemsButton->setFocusPolicy(Qt::NoFocus);

    auto menu = new QMenu(q);
    menu->setProperty(style::Properties::kMenuAsDropdown, true);
    menu->setWindowFlags(menu->windowFlags() | Qt::BypassGraphicsProxyWidget);

    const auto updateButtonText =
        [this](RightPanel::SystemSelection system)
        {
            m_filterSystemsButton->setText(m_systemSelectionActions[system]->text());
            m_filterSystemsButton->setState(system == RightPanel::SystemSelection::current
                    ? SelectableTextButton::State::deactivated
                    : SelectableTextButton::State::unselected);
           m_filterSystemsButton->setAccented(system == RightPanel::SystemSelection::organization);
        };


    const auto addMenuAction =
        [this, menu, updateButtonText](const QString& title, RightPanel::SystemSelection system)
        {
            auto action = menu->addAction(title);
            connect(action, &QAction::triggered, this,
                [this, updateButtonText, system]()
                {
                    if (system == RightPanel::SystemSelection::current)
                        m_filterModel->setFilterRegularExpression("^$");
                    else
                        m_filterModel->setFilterWildcard("*");

                    updateButtonText(system);
                });
            m_systemSelectionActions[system] = action;
            return action;
        };

    addMenuAction(tr("Current Site"), RightPanel::SystemSelection::current);
    addMenuAction({}, RightPanel::SystemSelection::organization);

    connect(
        system()->saasServiceManager(),
        &nx::vms::common::saas::ServiceManager::dataChanged,
        this,
        [this]()
        {
            if (!system()->saasServiceManager()->saasActive())
                return;

            m_systemSelectionActions[RightPanel::SystemSelection::organization]->setText(
                    system()->saasServiceManager()->data().organization.name);
        });

    connect(m_filterSystemsButton, &SelectableTextButton::stateChanged, this,
        [this](SelectableTextButton::State state)
        {
            if (state == SelectableTextButton::State::deactivated)
                m_systemSelectionActions[RightPanel::SystemSelection::current]->trigger();
        });

    m_systemSelectionActions[RightPanel::SystemSelection::current]->trigger();
    m_filterSystemsButton->setMenu(menu);
}

void NotificationListWidget::Private::setupFilterNotificationsButton()
{
    m_filterNotificationsButton->setFlat(true);
    m_filterNotificationsButton->setSelectable(false);
    m_filterNotificationsButton->setDeactivatable(true);
    m_filterNotificationsButton->setFocusPolicy(Qt::NoFocus);
    m_filterNotificationsButton->setSelectable(true);
    m_filterNotificationsButton->setDeactivatable(true);

    connect(
        m_filterNotificationsButton,
        &SelectableTextButton::stateChanged,
        this,
        &NotificationListWidget::Private::onFilterNotificationsButtonStateChanged);

    auto menu = new QMenu(q);
    menu->setProperty(style::Properties::kMenuAsDropdown, true);
    menu->setWindowFlags(menu->windowFlags() | Qt::BypassGraphicsProxyWidget);

    const auto addAction =
        [menu, this](const QString& text, auto handler)
        {
            connect(menu->addAction(text), &QAction::triggered, this, handler);
        };

    addAction(
        tr("Any Notification"),
        &NotificationListWidget::Private::onAnyNotificationActionTriggered);
    addAction(
        tr("Event Notifications"),
        &NotificationListWidget::Private::onEventNotificationsActionTriggered);
    addAction(
        tr("System Notifications"),
        &NotificationListWidget::Private::onSystemNotificationsActionTriggered);

    menu->addSeparator();

    addAction(
        tr("Choose Types..."),
        &NotificationListWidget::Private::onChooseTypesActionTriggered);

    m_filterNotificationsButton->setMenu(menu);

    updateFilterNotificationsButtonAppearance();
}

void NotificationListWidget::Private::changeHeaderItemsVisibilityIfNeeded()
{
    using namespace nx::vms::common;

    if (auto user = system()->user();
        user && user->isCloud() && qnCloudStatusWatcher->cloudSystems().size() > 1
        && saas::crossSiteNotificationsAllowed(system()))
    {
        m_filterSystemsButton->setVisible(true);
    }
    else
    {
        m_filterSystemsButton->setVisible(false);
    }
}

void NotificationListWidget::Private::onAnyNotificationActionTriggered()
{
    auto notificationSettingsManager = system()->userNotificationSettingsManager();
    notificationSettingsManager->setSettings(
        notificationSettingsManager->supportedEventTypes(),
        notificationSettingsManager->supportedMessageTypes());
}

void NotificationListWidget::Private::onEventNotificationsActionTriggered()
{
    auto notificationSettingsManager = system()->userNotificationSettingsManager();
    system()->userNotificationSettingsManager()->setSettings(
        notificationSettingsManager->supportedEventTypes(), {});
}

void NotificationListWidget::Private::onSystemNotificationsActionTriggered()
{
    auto notificationSettingsManager = system()->userNotificationSettingsManager();
    system()->userNotificationSettingsManager()->setSettings(
        {}, notificationSettingsManager->supportedMessageTypes());
}

void NotificationListWidget::Private::onChooseTypesActionTriggered()
{
    if (NX_ASSERT(m_notificationSettingsDialog))
        m_notificationSettingsDialog->exec();
}

void NotificationListWidget::Private::onFilterNotificationsButtonStateChanged()
{
    if (m_filterNotificationsButton->state() == SelectableTextButton::State::deactivated)
        onAnyNotificationActionTriggered();
}

void NotificationListWidget::Private::updateFilterNotificationsButtonAppearance()
{
    auto notificationSettingsManager = system()->userNotificationSettingsManager();
    const auto watchedEvents = notificationSettingsManager->watchedEvents();
    const auto watchedMessages = notificationSettingsManager->watchedMessages();

    if (watchedEvents == notificationSettingsManager->supportedEventTypes()
        && watchedMessages == notificationSettingsManager->supportedMessageTypes())
    {
        QSignalBlocker blocker{m_filterNotificationsButton};
        m_filterNotificationsButton->setText(tr("Any notification"));
        m_filterNotificationsButton->setState(SelectableTextButton::State::deactivated);

        return;
    }

    const auto watchedCount = watchedEvents.size() + watchedMessages.size();
    if (watchedCount == 1)
    {
        if (!watchedEvents.empty())
        {
            m_filterNotificationsButton->setText(m_stringHelper->eventName(watchedEvents.first()));
        }
        else
        {
            m_filterNotificationsButton->setText(
                QnSystemHealthStringsHelper::messageShortTitle(
                    system(), *watchedMessages.cbegin()));
        }
    }
    else
    {
        m_filterNotificationsButton->setText(
            tr("%n notification types", "", static_cast<int>(watchedCount)));
    }

    m_filterNotificationsButton->setState(SelectableTextButton::State::selected);
}

} // namespace nx::vms::client::desktop
