#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QVariant>
#include <QJSValue>
#include <QJSEngine>

#include <nx/utils/singleton.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchContext;

namespace nx::vmx::client::desktop {

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
    Q_INVOKABLE void subscribe(nx::vms::client::desktop::ui::action::IDType action, QJSValue callback);

    /** Invokes the specified action with parameters converted from js object properties. */
    Q_INVOKABLE void trigger(nx::vms::client::desktop::ui::action::IDType action, QJSValue parameters);

    /** Creates QnResourceList wrapped in QVariant from a list of files. */
    Q_INVOKABLE QVariant createResourcesForFiles(QStringList files);

    /** Gets QnResourcePtr by its id. Returns undefined if id is invalid or does not exist. */
    Q_INVOKABLE QVariant getResourceById(QString id);

    /** Returns a list of time points when client scene frames were rendered (in ms) */
    std::vector<qint64> getFrameTimePoints();

private:
    QJSEngine* m_engine = nullptr;
};

} // namespace nx::vmx::client::desktop
