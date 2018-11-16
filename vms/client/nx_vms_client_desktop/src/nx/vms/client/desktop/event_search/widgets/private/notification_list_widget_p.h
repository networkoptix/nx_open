#pragma once

#include "../notification_list_widget.h"

#include <QtCore/QScopedPointer>
#include <QtWidgets/QTabWidget>

#include <ui/workbench/workbench_context_aware.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>

class QToolButton;

namespace nx::vms::client::desktop {

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

    QWidget* m_placeholder = nullptr;
};

} // namespace nx::vms::client::desktop
