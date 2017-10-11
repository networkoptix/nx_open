#pragma once

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_fwd.h>

#include <client_core/connection_context_aware.h>

class QnWorkbenchContext;
class QnWorkbench;
class QnWorkbenchLayoutSnapshotManager;
class QnWorkbenchAccessController;
class QnWorkbenchDisplay;
class QnWorkbenchNavigator;
class QnWorkbenchCustomizer;

namespace nx {
namespace client {
namespace desktop {
namespace ui {

class MainWindow;

} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx

/**
 * This class simplifies access to workbench context.
 *
 * If some class needs access to workbench context,
 * it can simply derive from <tt>QnWorkbenchContextAware</tt> and
 * pass its context-aware parent into constructor.
 */
class QnWorkbenchContextAware: public QnConnectionContextAware
{
public:
    /**
     * Constructor.
     *
     * \param parent                    Parent for this object. Must itself be
     *                                  a child of some context-aware object,
     *                                  or of a context. Must not be NULL.
     */
    QnWorkbenchContextAware(QObject *parent, bool lazyInitialization = false);

    QnWorkbenchContextAware(QnWorkbenchContext *context);

    QnWorkbenchContextAware(QObject *parent, QnWorkbenchContext *context);

    /**
     * Virtual destructor.
     *
     * We do <tt>dynamic_cast</tt>s to <tt>QnWorkbenchContextAware</tt>, so this
     * class better have a vtable.
     */
    virtual ~QnWorkbenchContextAware() {}

    /**
     * \returns                         Context associated with this context-aware object.
     */
    QnWorkbenchContext *context() const {
        return m_context;
    }

    /** Delayed initialization call. */
    void initializeContext(QObject *parent);

    /** Delayed initialization call. */
    void initializeContext(QnWorkbenchContext *context);

protected:
    virtual void afterContextInitialized();

    QAction *action(const nx::client::desktop::ui::action::IDType id) const;

    nx::client::desktop::ui::action::Manager* menu() const;

    QnWorkbench *workbench() const;

    QnWorkbenchLayoutSnapshotManager *snapshotManager() const;

    QnWorkbenchAccessController *accessController() const;

    QnWorkbenchDisplay *display() const;

    QnWorkbenchNavigator *navigator() const;

    nx::client::desktop::ui::MainWindow* mainWindow() const;

    /** @return The same as mainWindow() but casted to QWidget*, so caller don't need to include
     * MainWindow header.
     */
    QWidget* mainWindowWidget() const;

private:
    void init(QObject *parent, bool lazyInitialization = false);
    void init(QnWorkbenchContext *context);

private:
    QnWorkbenchContext *m_context;
    bool m_initialized;
};
