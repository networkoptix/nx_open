#include "event_panel_p.h"

#include <QtGui/QPainter>
#include <QtWidgets/QMenu>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QVBoxLayout>

#include <core/resource/camera_resource.h>
#include <ui/common/notification_levels.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/style/custom_style.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>

#include <nx/client/desktop/common/widgets/tab_widget.h>
#include <nx/client/desktop/common/widgets/compact_tab_bar.h>
#include <nx/client/desktop/ui/common/selectable_text_button.h>
#include <nx/client/desktop/event_search/models/unified_async_search_list_model.h>
#include <nx/client/desktop/event_search/models/analytics_search_list_model.h>
#include <nx/client/desktop/event_search/models/bookmark_search_list_model.h>
#include <nx/client/desktop/event_search/models/event_search_list_model.h>
#include <nx/client/desktop/event_search/widgets/notification_list_widget.h>
#include <nx/client/desktop/event_search/widgets/unified_search_widget.h>
#include <nx/client/desktop/event_search/widgets/motion_search_widget.h>
#include <nx/client/desktop/event_search/widgets/notification_counter_label.h>
#include <nx/client/desktop/event_search/widgets/event_ribbon.h>
#include <nx/vms/event/strings_helper.h>

namespace nx {
namespace client {
namespace desktop {

namespace {

static constexpr int kPermanentTabCount = 1;

using ButtonState = ui::SelectableTextButton::State;

} // namespace

EventPanel::Private::Private(EventPanel* q):
    QObject(q),
    q(q),
    m_tabs(new TabWidget(new CompactTabBar(), q)),
    m_notificationsTab(new NotificationListWidget(m_tabs)),
    m_motionTab(new MotionSearchWidget(m_tabs)),
    m_bookmarksTab(new UnifiedSearchWidget(m_tabs)),
    m_eventsTab(new UnifiedSearchWidget(m_tabs)),
    m_analyticsTab(new UnifiedSearchWidget(m_tabs)),
    m_counterLabel(new NotificationCounterLabel(m_tabs->tabBar())),
    m_eventsModel(new EventSearchListModel(this)),
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

    setupEventSearch();
    setupBookmarkSearch();
    setupAnalyticsSearch();
}

EventPanel::Private::~Private()
{
}

void EventPanel::Private::addCameraTabs()
{
    NX_ASSERT(m_tabs->count() == kPermanentTabCount);

    m_tabs->addTab(m_motionTab, qnSkin->icon(lit("events/tabs/motion.png")),
        tr("Motion", "Motion tab title"));
    m_tabs->addTab(m_bookmarksTab, qnSkin->icon(lit("events/tabs/bookmarks.png")),
        tr("Bookmarks", "Bookmarks tab title"));
    m_tabs->addTab(m_eventsTab, qnSkin->icon(lit("events/tabs/events.png")),
        tr("Events", "Events tab title"));
    m_tabs->addTab(m_analyticsTab, qnSkin->icon(lit("events/tabs/analytics.png")),
        tr("Objects", "Analytics tab title"));
}

void EventPanel::Private::removeCameraTabs()
{
    while (m_tabs->count() > kPermanentTabCount)
        m_tabs->removeTab(kPermanentTabCount);
}

void EventPanel::Private::setupEventSearch()
{
    m_eventsTab->setModel(new UnifiedAsyncSearchListModel(m_eventsModel, this));
    m_eventsTab->setPlaceholderTexts(tr("No events"), tr("No events occured"));
    m_eventsTab->setPlaceholderIcon(qnSkin->pixmap(lit("events/placeholders/events.png")));

    auto button = m_eventsTab->typeButton();
    button->setIcon(qnSkin->icon(lit("text_buttons/event_rules.png")));
    button->show();

    auto eventFilterMenu = new QMenu(q);
    eventFilterMenu->setProperty(style::Properties::kMenuAsDropdown, true);
    eventFilterMenu->setWindowFlags(eventFilterMenu->windowFlags() | Qt::BypassGraphicsProxyWidget);

    auto addMenuAction =
        [this, eventFilterMenu](const QString& title, vms::event::EventType type)
        {
            auto action = eventFilterMenu->addAction(title);
            connect(action, &QAction::triggered, this,
                [this, title, type]()
                {
                    m_eventsTab->typeButton()->setText(title);
                    m_eventsTab->typeButton()->setState(type == vms::event::undefinedEvent
                        ? ButtonState::deactivated
                        : ButtonState::unselected);

                    m_eventsModel->setSelectedEventType(type);
                    m_eventsTab->requestFetch();
                });

            return action;
        };

    auto defaultAction = addMenuAction(tr("Any type"), vms::event::undefinedEvent);
    for (const auto type: vms::event::allEvents())
    {
        if (vms::event::isSourceCameraRequired(type))
            addMenuAction(m_helper->eventName(type), type);
    }

    connect(m_eventsTab->typeButton(), &ui::SelectableTextButton::stateChanged, this,
        [defaultAction](ButtonState state)
        {
            if (state == ButtonState::deactivated)
                defaultAction->trigger();
        });

    defaultAction->trigger();
    button->setMenu(eventFilterMenu);
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
}

void EventPanel::Private::setupAnalyticsSearch()
{
    m_analyticsTab->setModel(new UnifiedAsyncSearchListModel(m_analyticsModel, this));
    m_analyticsTab->setPlaceholderTexts(tr("No objects"), tr("No objects detected"));
    m_analyticsTab->setPlaceholderIcon(qnSkin->pixmap(lit("events/placeholders/analytics.png")));

    auto button = m_analyticsTab->areaButton();
    button->setDeactivatedText(tr("Anywhere on the video"));
    button->show();

    connect(button, &ui::SelectableTextButton::stateChanged, this,
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

    m_motionTab->setCamera(camera);
    m_eventsModel->setCamera(camera);
    m_bookmarksModel->setCamera(camera);
    m_analyticsModel->setCamera(camera);

    if (camera)
    {
        if (m_tabs->count() == kPermanentTabCount)
            addCameraTabs();
    }
    else
    {
        removeCameraTabs();
    }
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

    m_motionTab->setMotionSearchEnabled(false);

    if (!camera)
        return;

    m_mediaWidgetConnections.reset(new QnDisconnectHelper());

    *m_mediaWidgetConnections << connect(
        m_currentMediaWidget.data(), &QnMediaResourceWidget::motionSearchModeEnabled,
            m_motionTab, &MotionSearchWidget::setMotionSearchEnabled);

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

} // namespace desktop
} // namespace client
} // namespace nx
