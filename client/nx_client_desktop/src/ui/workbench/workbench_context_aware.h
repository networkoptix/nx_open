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
 * If some class needs access to workbench context, it can simply derive from
 * QnWorkbenchContextAware and pass its context-aware parent into constructor.
 */
class QnWorkbenchContextAware: public QnConnectionContextAware
{
public:
    static QString kQmlContextPropertyName;

    enum class InitializationMode
    {
        instant, /**< Initialize context from parent in constructor. */
        lazy, /**< Initialize context from parent when context() is called first time. */
        manual, /**< Initialize context later with manual call initializeContext(). */
        qmlContext, /**< Initialize context later using QML context property. */
    };

    /**
     * Constructor.
     *
     * @param parent Parent for this object. Must itself be a child of some context-aware object,
     *     or of a context. Must not be null. If lazyInitialization is true this can also be an
     *     object itself. In this case context initialization will be berformed when the first
     *     getter is called. The context will be found in QML context property.
     */
    QnWorkbenchContextAware(
        QObject* parent,
        InitializationMode initMode = InitializationMode::instant);

    QnWorkbenchContextAware(QnWorkbenchContext* context);

    /**
     * Virtual destructor.
     *
     * We do dynamic_casts to QnWorkbenchContextAware, so this class better have a vtable.
     */
    virtual ~QnWorkbenchContextAware();

    /**
     * @returns Context associated with this context-aware object.
     */
    QnWorkbenchContext* context() const;

    /** Check if context is initialized. */
    bool isContextInitialized() const;

    /** Initialize context manually. */
    void initializeContext();

protected:
    virtual void afterContextInitialized();

    QAction* action(const nx::client::desktop::ui::action::IDType id) const;

    nx::client::desktop::ui::action::Manager* menu() const;

    QnWorkbench* workbench() const;

    QnWorkbenchLayoutSnapshotManager* snapshotManager() const;

    QnWorkbenchAccessController* accessController() const;

    QnWorkbenchDisplay* display() const;

    QnWorkbenchNavigator* navigator() const;

    nx::client::desktop::ui::MainWindow* mainWindow() const;

    /**
     * @return The same as mainWindow() but casted to QWidget*, so caller don't need to include
     * MainWindow header.
     */
    QWidget* mainWindowWidget() const;

private:
    void init(QObject* parent);
    void init(QnWorkbenchContext* context);
    void initFromQmlContext();

private:
    QObject* m_parent = nullptr;
    InitializationMode m_initializationMode = InitializationMode::instant;
    QnWorkbenchContext* m_context = nullptr;
};

