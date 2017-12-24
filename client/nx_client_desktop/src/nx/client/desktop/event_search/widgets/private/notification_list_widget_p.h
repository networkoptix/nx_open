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
class AbstractEventListModel;

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
    EventRibbon* const m_eventRibbon = nullptr;
    AbstractEventListModel* m_systemHealthModel = nullptr;
    AbstractEventListModel* m_notificationsModel = nullptr;
};

} // namespace desktop
} // namespace client
} // namespace nx
