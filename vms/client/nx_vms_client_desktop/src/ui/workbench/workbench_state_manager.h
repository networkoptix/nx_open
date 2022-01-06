// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchState;

/** Delegate to maintain knowledge about current connection session. */
class QnSessionAwareDelegate: public QnWorkbenchContextAware
{
public:
    QnSessionAwareDelegate(
        QObject* parent = nullptr,
        InitializationMode initMode = InitializationMode::instant);
    ~QnSessionAwareDelegate();

    virtual bool tryClose(bool force) = 0;
    virtual void forcedUpdate() = 0;
};

template <typename T>
class QnBasicWorkbenchStateDelegate: public QnSessionAwareDelegate
{
public:
    QnBasicWorkbenchStateDelegate(T* owner):
        QnSessionAwareDelegate(owner),
        m_owner(owner)
    {
    }

    virtual bool tryClose(bool force) override
    {
        return m_owner->tryClose(force);
    }

    virtual void forcedUpdate() override
    {
        m_owner->forcedUpdate();
    }

private:
    T* m_owner;
};

class QnWorkbenchStateManager: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
public:
    QnWorkbenchStateManager(QObject *parent = nullptr);

    /** Forcibly update all delegates.
     *  Should be called in case of full state re-read (reconnect, merge systems, etc).
     **/
    void forcedUpdate();

    bool tryClose(bool force);

private:
    friend class QnSessionAwareDelegate;

    void registerDelegate(QnSessionAwareDelegate* d);
    void unregisterDelegate(QnSessionAwareDelegate* d);

private:
    QList<QnSessionAwareDelegate*> m_delegates;
};
