// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <nx/vms/client/desktop/menu/action_types.h>
#include <nx/vms/client/desktop/menu/actions.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchSelectionWatcher: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef QObject base_type;
public:
    QnWorkbenchSelectionWatcher(QObject *parent = nullptr);
    virtual ~QnWorkbenchSelectionWatcher();

    nx::vms::client::desktop::menu::ActionScopes scope() const;
    void setScope(nx::vms::client::desktop::menu::ActionScopes value);

private:
    void updateFromSelection();

signals:
    void selectionChanged(const QnResourceList &resources);

private:
    nx::vms::client::desktop::menu::ActionScopes m_scope;

    /** Whether the set of selected resources were changed. */
    bool m_selectionUpdatePending;

    /** Scope of the last selection change. */
    nx::vms::client::desktop::menu::ActionScope m_lastScope;
};
