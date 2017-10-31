#include "notification_list_widget_p.h"

#include <QtWidgets/QAction>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QToolButton>

#include <business/business_resource_validation.h>
#include <client/client_settings.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <utils/common/event_processors.h>
#include <ui/common/custom_painted.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/statistics/modules/controls_statistics_module.h>
#include <ui/style/skin.h>
#include <ui/workbench/workbench_context.h>

#include <nx/client/desktop/event_search/widgets/event_ribbon.h>
#include <nx/client/desktop/event_search/widgets/event_tile.h>
#include <nx/client/desktop/event_search/models/system_health_list_model.h>
#include <nx/client/desktop/event_search/models/notification_list_model.h>
#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/events/abstract_event.h>
#include <nx/vms/event/strings_helper.h>

namespace nx {
namespace client {
namespace desktop
{

NotificationListWidget::Private::Private(NotificationListWidget* q) :
    QObject(),
    QnWorkbenchContextAware(q),
    q(q),
    m_systemHealth(new EventRibbon(q)),
    m_notifications(new EventRibbon(q)),
    m_systemHealthModel(new SystemHealthListModel(this)),
    m_notificationsModel(new NotificationListModel(this))
{
    auto headerWidget = new QWidget(q);
    auto headerLayout = new QHBoxLayout(headerWidget);
    headerLayout->addStretch();
    headerLayout->addWidget(newActionButton(ui::action::OpenBusinessLogAction,
        Qn::MainWindow_Notifications_EventLog_Help));
    headerLayout->addWidget(newActionButton(ui::action::BusinessEventsAction, -1));
    headerLayout->addWidget(newActionButton(ui::action::PreferencesNotificationTabAction, -1));

    auto layout = new QVBoxLayout(q);
    layout->setSpacing(0);
    layout->addWidget(headerWidget);
    layout->addWidget(m_systemHealth);
    layout->addWidget(m_notifications);

    m_systemHealth->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_notifications->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    const auto handleActions =
        [this](EventRibbon* ribbon, EventListModel* model)
        {
            connect(ribbon, &EventRibbon::clicked, model, &EventListModel::defaultAction);
            connect(ribbon, &EventRibbon::closeRequested, model, &EventListModel::closeAction);
            connect(ribbon, &EventRibbon::linkActivated, model, &EventListModel::linkAction);
        };

    m_systemHealth->setModel(m_systemHealthModel);
    m_notifications->setModel(m_notificationsModel);

    handleActions(m_systemHealth, m_systemHealthModel);
    handleActions(m_notifications, m_notificationsModel);
}

NotificationListWidget::Private::~Private()
{
}

QToolButton* NotificationListWidget::Private::newActionButton(
    ui::action::IDType actionId,
    int helpTopicId)
{
    const auto paintFunction =
        [](QPainter* painter, const QStyleOption* /*option*/, const QWidget* widget) -> bool
        {
            const auto button = qobject_cast<const QToolButton*>(widget);
            if (!button)
                return false;

            auto mode = QIcon::Normal;
            if (button->isDown())
                mode = QnIcon::Pressed;
            else if (button->underMouse())
                mode = QIcon::Active;

            button->icon().paint(painter, button->rect(), Qt::AlignCenter, mode);
            return true;
        };

    auto button = new CustomPainted<QToolButton>();
    button->setCustomPaintFunction(paintFunction);
    button->setDefaultAction(action(actionId));
    button->setIconSize(QnSkin::maximumSize(button->defaultAction()->icon()));
    button->setFixedSize(button->iconSize());

    if (helpTopicId != Qn::Empty_Help)
        setHelpTopic(button, helpTopicId);

    connect(context(), &QnWorkbenchContext::userChanged, this,
        [this, button, actionId]()
        {
            button->setVisible(menu()->canTrigger(actionId));
        });

    button->setVisible(menu()->canTrigger(actionId));

    const auto statAlias = lit("%1_%2").arg(lit("notifications_collection_widget"),
        QnLexical::serialized(actionId));

    context()->statisticsModule()->registerButton(statAlias, button);
    return button;
}

} // namespace
} // namespace client
} // namespace nx
