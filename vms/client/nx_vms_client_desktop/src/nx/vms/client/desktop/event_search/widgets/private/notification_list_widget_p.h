#pragma once

#include "../notification_list_widget.h"

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
    NotificationListWidget* const q;
    EventRibbon* const m_eventRibbon;
    QWidget* const m_placeholder;
    AbstractEventListModel* const m_systemHealthModel;
    AbstractEventListModel* const m_notificationsModel;
};

} // namespace nx::vms::client::desktop
