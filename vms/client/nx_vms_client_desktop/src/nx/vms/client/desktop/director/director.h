// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QVariant>
#include <QtQml/QJSValue>
#include <QtQml/QJSEngine>

#include <nx/utils/singleton.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchContext;

namespace nx::vms::client::desktop {

/**
 * Director provides API for high-level operational control of NX client.
 * It is used (or to be used) for functional testing, video wall remote, scripting etc.
 */

class Director:
    public QObject,
    public QnWorkbenchContextAware,
    public Singleton<Director>
{
    Q_OBJECT
public:
    explicit Director(QObject* parent);
    virtual ~Director();

    /** Registers necessary global objects within QJSEngine instance. */
    void setupJSEngine(QJSEngine* engine);

    /** Closes client application. */
    Q_INVOKABLE void quit(bool force = true);

    /**
     * Registers a js callback function to call when specified action happens.
     * Action parameters are passed as a single js object parameter to callback.
     */
    Q_INVOKABLE void subscribe(ui::action::IDType action, QJSValue callback);

    /** Invokes the specified action with parameters converted from js object properties. */
    Q_INVOKABLE void trigger(ui::action::IDType action, QJSValue parameters);

    /** Get all resources from ResourcePool as parsed JSON. */
    Q_INVOKABLE QJSValue getResources() const;

    /** Execute provided JavaScript code. */
    QJSValue execute(const QString& source);

    /** Returns a list of time points when client scene frames were rendered (in ms) */
    std::vector<qint64> getFrameTimePoints();

private:
    QJSEngine* m_engine = nullptr;
};

} // namespace nx::vms::client::desktop
