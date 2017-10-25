#pragma once

#include "../notification_list_widget.h"

#include <QtCore/QScopedPointer>
#include <QtWidgets/QTabWidget>

#include <ui/workbench/workbench_context_aware.h>
#include <nx/client/desktop/ui/actions/actions.h>

class QToolButton;
class QnDisconnectHelper;

namespace nx {
namespace client {
namespace desktop {

class EventTile;
class EventRibbon;

class NotificationListWidget::Private:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    Private(NotificationListWidget* q);
    virtual ~Private() override;

private:
    QToolButton* newActionButton(ui::action::IDType actionId, int helpTopicId);

private:
    NotificationListWidget* q = nullptr;
    EventRibbon* const m_systemHealth = nullptr;
    EventRibbon* const m_notifications = nullptr;
    EventListModel* m_systemHealthModel = nullptr;
    EventListModel* m_notificationsModel = nullptr;
};

} // namespace
} // namespace client
} // namespace nx
