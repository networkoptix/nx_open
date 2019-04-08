#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <nx/utils/singleton.h>
#include <ui/workbench/workbench_context_aware.h>

class QnWorkbenchContext;

namespace nx::vmx::client::desktop {

/**
 * Director provides API for high-level operational control of NX client.
 * It is used (or to be used) for functional testing, video wall remote etc.
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

    /** Closes client application. */
    void quit(bool force = true);

    /** Returns a list of time points when client scene frames were rendered (in ms) */
    std::vector<qint64> getFrameTimePoints();
};

} // namespace nx::vmx::client::desktop
