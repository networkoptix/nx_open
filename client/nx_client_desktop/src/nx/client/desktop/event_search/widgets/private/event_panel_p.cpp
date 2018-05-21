#include "event_panel_p.h"

#include <QtGui/QPainter>
#include <QtWidgets/QMenu>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>

#include <core/resource/camera_resource.h>
#include <ui/common/notification_levels.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/style/custom_style.h>
#include <ui/style/skin.h>
#include <nx/client/desktop/common/widgets/search_line_edit.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_navigator.h>

#include <nx/client/desktop/common/widgets/animated_tab_widget.h>
#include <nx/client/desktop/common/widgets/compact_tab_bar.h>
#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/common/widgets/selectable_text_button.h>
#include <nx/client/desktop/event_search/models/unified_async_search_list_model.h>
#include <nx/client/desktop/event_search/models/analytics_search_list_model.h>
#include <nx/client/desktop/event_search/models/bookmark_search_list_model.h>
#include <nx/client/desktop/event_search/models/motion_search_list_model.h>
#include <nx/client/desktop/event_search/models/event_search_list_model.h>
#include <nx/client/desktop/event_search/widgets/notification_list_widget.h>
#include <nx/client/desktop/event_search/widgets/unified_search_widget.h>
#include <nx/client/desktop/event_search/widgets/notification_counter_label.h>
#include <nx/client/desktop/event_search/widgets/event_ribbon.h>
#include <nx/vms/event/strings_helper.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static constexpr int kPermanentTabCount = 1;

using ButtonState = SelectableTextButton::State;

} // namespace

EventPanel::Private::Private(EventPanel* q):
    QObject(q),
    q(q),
    m_tabs(new AnimatedTabWidget(new CompactTabBar(), q)),
    m_notificationsTab(new NotificationListWidget(m_tabs)),
    m_motionTab(new UnifiedSearchWidget(m_tabs)),
    m_bookmarksTab(new UnifiedSearchWidget(m_tabs)),
    m_eventsTab(new UnifiedSearchWidget(m_tabs)),
    m_analyticsTab(new UnifiedSearchWidget(m_tabs)),
    m_counterLabel(new NotificationCounterLabel(m_tabs->tabBar())),
    m_eventsModel(new EventSearchListModel(this)),
    m_motionModel(new MotionSearchListModel(this)),
    m_bookmarksModel(new BookmarkSearchListModel(this)),
    m_analyticsModel(new AnalyticsSearchListModel(this)),
    m_helper(new vms::event::StringsHelper(q->commonModule()))
{
    auto layout = new QVBoxLayout(q);
    layout->setContentsMargins(QMargins());
    layout->addWidget(m_tabs);
    m_tabs->addTab(m_notificationsTab, qnSkin->icon(lit("events/tabs/notifications.png")),
        tr("Notifications", "Notifications tab title"));

    connect(m_notificationsTab, &NotificationListWidget::unreadCountChanged,
        this, &Private::updateUnreadCounter);

    connect(m_tabs->tabBar(), &QTabBar::tabBarClicked, this,
        [this](int index)
        {
            if (m_tabs->currentIndex() != index || !m_tabs->currentWidget())
                return;

            if (auto ribbon = m_tabs->currentWidget()->findChild<EventRibbon*>())
                ribbon->scrollBar()->setValue(0);
        });

    m_motionTab->hide();
    m_bookmarksTab->hide();
    m_eventsTab->hide();
    m_analyticsTab->hide();
    m_counterLabel->hide();

    static constexpr int kTabBarShift = 10;
    m_tabs->setProperty(style::Properties::kTabBarIndent, kTabBarShift);
    setTabShape(m_tabs->tabBar(), style::TabShape::Compact);

    q->setAutoFillBackground(false);
    q->setAttribute(Qt::WA_TranslucentBackground);

    connect(q->context()->display(), &QnWorkbenchDisplay::widgetChanged,
        this, &Private::currentWorkbenchWidgetChanged, Qt::QueuedConnection);

    setupMotionSearch();
    setupBookmarkSearch();
    setupEventSearch();
    setupAnalyticsSearch();

    connect(m_notificationsTab, &NotificationListWidget::tileHovered, q, &EventPanel::tileHovered);

    for (auto tab: {m_eventsTab, m_motionTab, m_bookmarksTab, m_analyticsTab})
        connect(tab, &UnifiedSearchWidget::tileHovered, q, &EventPanel::tileHovered);

    setupTabsSyncWithNavigator();
}

EventPanel::Private::~Private() = default;

void EventPanel::Private::updateTabs()
{
    const bool hasVideo = m_camera && m_camera->hasVideo();
    const bool hasMotion = hasVideo && m_camera->hasFlags(Qn::motion);

    int nextTabIndex = kPermanentTabCount;

    const auto updateTab =
        [this, &nextTabIndex](QWidget* tab, bool visible, const QIcon& icon, const QString& text)
        {
            const int index = m_tabs->indexOf(tab);
            if (visible)
            {
                if (index < 0)
                    m_tabs->insertTab(nextTabIndex, tab, icon, text);
                else
                    NX_ASSERT(index == nextTabIndex);

                ++nextTabIndex;
            }
            else
            {
                if (index >= 0)
                {
                    NX_ASSERT(index == nextTabIndex);
                    m_tabs->removeTab(index);
                }
            }
        };

    updateTab(m_motionTab, hasMotion, qnSkin->icon(lit("events/tabs/motion.png")),
        tr("Motion", "Motion tab title"));

    updateTab(m_bookmarksTab, m_camera != nullptr, qnSkin->icon(lit("events/tabs/bookmarks.png")),
        tr("Bookmarks", "Bookmarks tab title"));

    updateTab(m_eventsTab, m_camera != nullptr, qnSkin->icon(lit("events/tabs/events.png")),
        tr("Events", "Events tab title"));

    updateTab(m_analyticsTab, hasVideo, qnSkin->icon(lit("events/tabs/analytics.png")),
        tr("Objects", "Analytics tab title"));
}

void EventPanel::Private::setupMotionSearch()
{
    auto model = new UnifiedAsyncSearchListModel(m_motionModel, this);
    m_motionTab->setModel(model);
    m_motionTab->setPlaceholderIcon(qnSkin->pixmap(lit("events/placeholders/motion.png")));

    m_motionTab->filterEdit()->hide();
    m_motionTab->showPreviewsButton()->show();

    connect(m_motionModel, &MotionSearchListModel::totalCountChanged, this,
        [this](int totalCount)
        {
            m_motionTab->counterLabel()->setText(totalCount
                ? tr("%n motion events", "", totalCount)
                : QString());
        });
}

void EventPanel::Private::setupBookmarkSearch()
{
    static const QString kHtmlPlaceholder =
        lit("<center><p>%1</p><p><font size='-3'>%2</font></p></center>")
            .arg(tr("No bookmarks"))
            .arg(tr("Select some period on timeline and click "
                "with right mouse button on it to create a bookmark."));
    m_bookmarksTab->setModel(new UnifiedAsyncSearchListModel(m_bookmarksModel, this));
    m_bookmarksTab->setPlaceholderTexts(tr("No bookmarks"), kHtmlPlaceholder);
    m_bookmarksTab->setPlaceholderIcon(qnSkin->pixmap(lit("events/placeholders/bookmarks.png")));
    m_bookmarksTab->showPreviewsButton()->show();

    connect(m_bookmarksTab->filterEdit(), &SearchLineEdit::textChanged, m_bookmarksModel,
        [this](const QString& text)
        {
            m_bookmarksModel->setFilterText(text);
            m_bookmarksTab->requestFetch();
        });

    m_bookmarksTab->counterLabel()->setText(QString());
    connectToRowCountChanges(m_bookmarksModel,
        [this]()
        {
            const auto count = m_bookmarksModel->rowCount();
            m_bookmarksTab->counterLabel()->setText(count > 0
                ? (count > 99 ? tr(">99 bookmarks") : tr("%n bookmarks", "", count))
                : QString());
        });
}

void EventPanel::Private::setupEventSearch()
{
    auto model = new UnifiedAsyncSearchListModel(m_eventsModel, this);
    m_eventsTab->setModel(model);
    m_eventsTab->setPlaceholderTexts(tr("No events"), tr("No events occured"));
    m_eventsTab->setPlaceholderIcon(qnSkin->pixmap(lit("events/placeholders/events.png")));

    connect(m_eventsTab->filterEdit(), &SearchLineEdit::textChanged,
        model, &UnifiedAsyncSearchListModel::setClientsideTextFilter);

    auto button = m_eventsTab->typeButton();
    button->setIcon(qnSkin->icon(lit("text_buttons/event_rules.png")));
    button->show();

    auto eventFilterMenu = new QMenu(q);
    eventFilterMenu->setProperty(style::Properties::kMenuAsDropdown, true);
    eventFilterMenu->setWindowFlags(eventFilterMenu->windowFlags() | Qt::BypassGraphicsProxyWidget);

    auto addMenuAction =
        [this, eventFilterMenu](const QString& title, vms::api::EventType type)
        {
            auto action = eventFilterMenu->addAction(title);
            connect(action, &QAction::triggered, this,
                [this, title, type]()
                {
                    m_eventsTab->typeButton()->setText(title);
                    m_eventsTab->typeButton()->setState(type == vms::api::EventType::undefinedEvent
                        ? ButtonState::deactivated
                        : ButtonState::unselected);

                    m_eventsModel->setSelectedEventType(type);
                    m_eventsTab->requestFetch();
                });

            return action;
        };

    auto defaultAction = addMenuAction(tr("Any type"), vms::api::EventType::undefinedEvent);
    for (const auto type: vms::event::allEvents())
    {
        if (vms::event::isSourceCameraRequired(type))
            addMenuAction(m_helper->eventName(type), type);
    }

    connect(m_eventsTab->typeButton(), &SelectableTextButton::stateChanged, this,
        [defaultAction](ButtonState state)
        {
            if (state == ButtonState::deactivated)
                defaultAction->trigger();
        });

    defaultAction->trigger();
    button->setMenu(eventFilterMenu);

    m_eventsTab->counterLabel()->setText(QString());
    connectToRowCountChanges(m_eventsModel,
        [this]()
        {
            const auto count = m_eventsModel->rowCount();
            m_eventsTab->counterLabel()->setText(count
                ? (count > 99 ? tr(">99 events") : tr("%n events", "", count))
                : QString());
        });
}

void EventPanel::Private::setupAnalyticsSearch()
{
    m_analyticsTab->setModel(new UnifiedAsyncSearchListModel(m_analyticsModel, this));
    m_analyticsTab->setPlaceholderTexts(tr("No objects"), tr("No objects detected"));
    m_analyticsTab->setPlaceholderIcon(qnSkin->pixmap(lit("events/placeholders/analytics.png")));
    m_analyticsTab->showPreviewsButton()->show();
    m_analyticsTab->showInfoButton()->show();

    connect(m_analyticsTab->filterEdit(), &SearchLineEdit::textChanged, m_analyticsModel,
        [this](const QString& text)
        {
            m_analyticsModel->setFilterText(text);
            m_analyticsTab->requestFetch();
        });

    auto button = m_analyticsTab->areaButton();
    button->setDeactivatedText(tr("Anywhere on the video"));
    button->show();

    connect(button, &SelectableTextButton::stateChanged, this,
        [this, button](ButtonState state)
        {
            if (!m_currentMediaWidget)
                return;

            m_currentMediaWidget->setAnalyticsSearchModeEnabled(
                state != ButtonState::deactivated);

            if (state == ButtonState::selected)
                button->setText(tr("Select some area on video"));
            else if (state == ButtonState::unselected)
                button->setText(tr("In selected area"));

            if (state == ButtonState::deactivated)
                m_currentMediaWidget->setAnalyticsSearchRect(QRectF());
    });

    m_analyticsTab->counterLabel()->setText(QString());
    connectToRowCountChanges(m_analyticsModel,
        [this]()
        {
            const auto count = m_analyticsModel->rowCount();
            m_analyticsTab->counterLabel()->setText(count
                ? (count > 99 ? tr(">99 detected objects") : tr("%n detected objects", "", count))
                : QString());
        });
}

QnVirtualCameraResourcePtr EventPanel::Private::camera() const
{
    return m_camera;
}

void EventPanel::Private::setCamera(const QnVirtualCameraResourcePtr& camera)
{
    if (m_camera == camera)
        return;

    m_camera = camera;

    m_eventsModel->setCamera(camera);
    m_motionModel->setCamera(camera);
    m_bookmarksModel->setCamera(camera);
    m_analyticsModel->setCamera(camera);

    updateTabs();
}

void EventPanel::Private::currentWorkbenchWidgetChanged(Qn::ItemRole role)
{
    if (role != Qn::CentralRole)
        return;

    m_mediaWidgetConnections.reset();

    m_currentMediaWidget = qobject_cast<QnMediaResourceWidget*>(
        this->q->context()->display()->widget(Qn::CentralRole));

    const auto camera = m_currentMediaWidget
        ? m_currentMediaWidget->resource().dynamicCast<QnVirtualCameraResource>()
        : QnVirtualCameraResourcePtr();

    setCamera(camera);

    if (!camera)
        return;

    m_mediaWidgetConnections.reset(new QnDisconnectHelper());

    *m_mediaWidgetConnections << connect(
        m_currentMediaWidget.data(), &QnMediaResourceWidget::analyticsSearchAreaSelected, this,
        [this](const QRectF& relativeRect)
        {
            m_analyticsModel->setFilterRect(relativeRect);
            m_analyticsTab->areaButton()->setState(relativeRect.isValid()
                ? ButtonState::unselected
                : ButtonState::deactivated);

            m_analyticsTab->requestFetch();
        });

    *m_mediaWidgetConnections << connect(m_currentMediaWidget.data(),
        &QnMediaResourceWidget::motionSearchModeEnabled, this, &Private::at_motionSearchToggled);

    *m_mediaWidgetConnections << connect(m_currentMediaWidget.data(),
        &QnMediaResourceWidget::motionSelectionChanged, this, &Private::at_motionSelectionChanged);

    at_motionSelectionChanged();
    at_specialModeToggled(m_currentMediaWidget->isMotionSearchModeEnabled(), m_motionTab);
}

void EventPanel::Private::updateUnreadCounter(int count, QnNotificationLevel::Value importance)
{
    m_counterLabel->setVisible(count > 0);
    if (count == 0)
        return;

    const auto text = (count > 99) ? lit("99+") : QString::number(count);
    const auto color = QnNotificationLevel::notificationTextColor(importance);

    m_counterLabel->setText(text);
    m_counterLabel->setBackgroundColor(color);

    const auto width = m_counterLabel->minimumSizeHint().width();

    static constexpr int kTopMargin = 6;
    static constexpr int kRightBoundaryPosition = 32;
    m_counterLabel->setGeometry(kRightBoundaryPosition - width, kTopMargin,
        width, m_counterLabel->minimumHeight());
}

void EventPanel::Private::setupTabsSyncWithNavigator()
{
    connect(m_tabs, &QTabWidget::currentChanged, this,
        [this](int index)
        {
            q->context()->action(ui::action::BookmarksModeAction)->setChecked(
                m_tabs->currentWidget() == m_bookmarksTab);

            if (m_currentMediaWidget)
            {
                QnResourceWidgetList widgets({m_currentMediaWidget});
                if (m_tabs->currentWidget() == m_motionTab)
                    q->menu()->trigger(ui::action::StartSmartSearchAction, widgets);
                else
                    q->menu()->trigger(ui::action::StopSmartSearchAction, widgets);
            }

            auto extraContent = Qn::RecordingContent;
            if (m_tabs->currentWidget() == m_motionTab)
                extraContent = Qn::MotionContent;
            else if (m_tabs->currentWidget() == m_analyticsTab)
                extraContent = Qn::AnalyticsContent;

            q->navigator()->setSelectedExtraContent(extraContent);

            m_previousTabIndex = m_lastTabIndex;
            m_lastTabIndex = index;
        });

    connect(q->context()->action(ui::action::BookmarksModeAction), &QAction::toggled,
        this, &Private::at_bookmarksToggled);
}

void EventPanel::Private::at_motionSearchToggled(bool on)
{
    at_specialModeToggled(on, m_motionTab);
}

void EventPanel::Private::at_bookmarksToggled(bool on)
{
    at_specialModeToggled(on, m_bookmarksTab);
}

void EventPanel::Private::at_specialModeToggled(bool on, QWidget* correspondingTab)
{
    const auto index = m_tabs->indexOf(correspondingTab);
    if (index < 0)
        return;

    if (on)
    {
        m_tabs->setCurrentIndex(index);
    }
    else
    {
        if (m_tabs->currentWidget() == correspondingTab)
            m_tabs->setCurrentIndex(m_previousTabIndex);
    }
}

void EventPanel::Private::at_motionSelectionChanged()
{
    if (!m_currentMediaWidget || m_currentMediaWidget->isMotionSelectionEmpty())
    {
        static const QString kHtmlPlaceholder =
            lit("<center><p>%1</p><p><font size='-3'>%2</font></p></center>")
                .arg(tr("No motion region"))
                .arg(tr("Select some area on camera."));

        m_motionTab->setPlaceholderTexts(QString(), kHtmlPlaceholder);
    }
    else
    {
        m_motionTab->setPlaceholderTexts(tr("No motion"), tr("No motion detected"));
    }
}

void EventPanel::Private::connectToRowCountChanges(QAbstractItemModel* model,
    std::function<void()> handler)
{
    connect(model, &QAbstractItemModel::modelReset, this, handler);
    connect(model, &QAbstractItemModel::rowsInserted, this, handler);
    connect(model, &QAbstractItemModel::rowsRemoved, this, handler);
}

} // namespace desktop
} // namespace client
} // namespace nx
