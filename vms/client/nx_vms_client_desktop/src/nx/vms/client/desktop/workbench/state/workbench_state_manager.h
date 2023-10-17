// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/client/desktop/window_context_aware.h>

namespace nx::vms::client::desktop {

class WorkbenchStateManager;

/** Delegate to maintain knowledge about current connection session. */
class SessionAwareDelegate
{
public:
    SessionAwareDelegate(WorkbenchStateManager* manager);
    virtual ~SessionAwareDelegate();

    virtual bool tryClose(bool force) = 0;

private:
    WorkbenchStateManager* m_manager;
};

class WorkbenchStateManager: public QObject, public WindowContextAware
{
    Q_OBJECT

public:
    WorkbenchStateManager(WindowContext* windowContext, QObject* parent = nullptr);

    bool tryClose(bool force);

private:
    friend class SessionAwareDelegate;

    void registerDelegate(SessionAwareDelegate* d);
    void unregisterDelegate(SessionAwareDelegate* d);

private:
    QList<SessionAwareDelegate*> m_delegates;
};

} // namespace nx::vms::client::desktop
