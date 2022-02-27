// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include "../notification_list_widget.h"

#include <QtWidgets/QTabWidget>

#include <ui/workbench/workbench_context_aware.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>

class QToolButton;

namespace nx::vms::client::desktop {

class EventRibbon;
class NotificationTabModel;

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
    NotificationTabModel* const m_model;
};

} // namespace nx::vms::client::desktop
