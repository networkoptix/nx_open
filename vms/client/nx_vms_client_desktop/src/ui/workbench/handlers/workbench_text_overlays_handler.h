// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/event/event_fwd.h>
#include <nx/vms/rules/action_executor.h>
#include <ui/workbench/workbench_context_aware.h>

class QnResourceWidget;
class QnWorkbenchTextOverlaysHandlerPrivate;

namespace nx::vms::event { class StringsHelper; }

class QnWorkbenchTextOverlaysHandler:
    public nx::vms::rules::ActionExecutor,
    public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = nx::vms::rules::ActionExecutor;

public:
    QnWorkbenchTextOverlaysHandler(QObject* parent = nullptr);
    virtual ~QnWorkbenchTextOverlaysHandler();

private:
    void at_eventActionReceived(const nx::vms::event::AbstractActionPtr& businessAction);
    void at_resourceWidgetAdded(QnResourceWidget* widget);

    virtual void execute(const nx::vms::rules::ActionPtr& action);

private:
    Q_DECLARE_PRIVATE(QnWorkbenchTextOverlaysHandler);
    const QScopedPointer<QnWorkbenchTextOverlaysHandlerPrivate> d_ptr;
    QScopedPointer<nx::vms::event::StringsHelper> m_helper;
};
