#pragma once

#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QVariant>
#include <QtCore/QPointer>

#include <core/resource/resource_fwd.h>
#include <core/resource/layout_item_index.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_fwd.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx {
namespace client {
namespace desktop {
namespace ui {
namespace action {

/**
 * Action manager stores application's actions and presents an interface for
 * creating context menus given current action scope and parameters.
 */
class Manager: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT;
public:
    enum CreationOption
    {
        DontReuseActions = 0x1
    };
    Q_DECLARE_FLAGS(CreationOptions, CreationOption)

    /**
     * Default constructor.
     *
     * \param parent                    Context-aware parent of this action manager.
     */
    Manager(QObject* parent = nullptr);

    /**
     * Virtual destructor.
     */
    virtual ~Manager();

    /**
     * \param action                    New action to register with this action manager.
     */
    void registerAction(Action* action);

    /**
     * Registers action id as an alias for another action id.
     *
     * \param sourceId                  Alias id.
     * \param targetId                  Id of the target action.
     */
    void registerAlias(IDType id, IDType targetId);

    /**
     * \param id                        Id of the action to get.
     * \returns                         Action for the given id, or nullptr if no such action exists.
     */
    Action* action(IDType id) const;

    /**
     * \returns                         List of all actions of this action manager.
     */
    QList<Action*> actions() const;

    /**
     * \param id                        Id of the action to check.
     * \param parameters                Parameters to check.
     * \returns                         Whether the action with the given id can
     *                                  be triggered with supplied parameters.
     */
    bool canTrigger(IDType id, const Parameters& parameters = Parameters());

    /**
     * Triggers the action with the given id.
     *
     * \param id                        Id of the action to trigger.
     * \param parameters                Parameters to pass to action handler.
     */
    void trigger(IDType id, const Parameters& parameters = Parameters());


    /**
     * Triggers the action with the given id if possible
     *
     * \param id                        Id of the action to trigger.
     * \param parameters                Parameters to pass to action handler.
     * \returns                         Was action triggered or not.
     */
    bool triggerIfPossible(IDType id, const Parameters& parameters = Parameters());

    /**
     * \param scope                     Scope of the menu to create.
     * \param parameters                Action parameters for menu creation.
     * \param parent                    Parent widget for the new menu.
     * \returns                         Newly created menu for the given paramters.
     *                                  Ownership of the created menu is passed to
     *                                  the caller.
     */
    QMenu* newMenu(
        ActionScope scope,
        QWidget* parent = nullptr,
        const Parameters& parameters = Parameters(),
        CreationOptions options = 0);

    QMenu* newMenu(
        IDType rootId,
        ActionScope scope,
        QWidget* parent = nullptr,
        const Parameters& parameters = Parameters(),
        CreationOptions options = 0);

    /**
     * \returns                         Action target provider that is assigned to this
     *                                  action manager.
     */
    TargetProvider* targetProvider() const;

    /**
     * \param targetProvider            New target provider for this action manager.
     */
    void setTargetProvider(TargetProvider* targetProvider);

    /**
     * \param action                    Action that has just been activated.
     * \returns                         Parameters with which the given action
     *                                  was triggered.
     */
    Parameters currentParameters(Action* action) const;

    /**
     * This is a convenience overload to be used inside handlers of <tt>QAction</tt>'s
     * <tt>triggered()</tt> signal.
     *
     * \param sender                    Action that has just been activated.
     * \returns                         Parameters with which the given action
     *                                  was triggered.
     */
    Parameters currentParameters(QObject* sender) const;

    /**
     * This function replaces one action in the given menu with another one.
     *
     * \param menu                      Menu to replace the action in.
     * \param targetId                  Id of the action to be replaced.
     * \param targetAction              Replacement action.
     */
    void redirectAction(QMenu* menu, IDType sourceId, QAction* targetAction);

    /** Check if any menu is visible right now */
    bool isMenuVisible() const;
signals:
    void menuAboutToShow(QMenu* menu);
    void menuAboutToHide(QMenu* menu);

    void actionRegistered(IDType id);

protected:
    friend class Action;

    virtual bool eventFilter(QObject* watched, QEvent* event) override;

    void copyAction(QAction* dst, Action* src, bool forwardSignals = true);

    QMenu* newMenuRecursive(
        const Action* parent,
        ActionScope scope,
        const Parameters& parameters,
        QWidget* parentWidget,
        CreationOptions options);

    bool redirectActionRecursive(QMenu* menu, IDType targetId, QAction* targetAction);

    /** Setup proper connections between menu and the manager. */
    QMenu* integrateMenu(QMenu* menu, const Parameters& parameters);

    /** Hide all menus that are currently opened. */
    void hideAllMenus();

private slots:
    void at_menu_destroyed(QObject* menuObj);

private:
    /** Root action. Also contained in the maps. */
    Action* m_root;

    /** Mapping from action id to action instance. */
    QHash<IDType, Action*> m_actionById;

    /** Mapping from action to action id. */
    QHash<Action*, IDType> m_idByAction;

    /** Mapping from a menu created by this manager to the parameters that were
     * passed to it at construction time. nullptr key is used for shortcut actions. */
    QHash<QMenu*, Parameters> m_parametersByMenu;

    /** Target provider for actions. */
    TargetProvider* m_targetProvider;

    /** Guard for target provider. */
    QPointer<QObject> m_targetProviderGuard;

    /** Currently active action that was activated via a shortcut. */
    Action* m_shortcutAction;

    /** Last menu that was clicked by the user. */
    QMenu* m_lastClickedMenu;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Manager::CreationOptions)

} // namespace action
} // namespace ui
} // namespace desktop
} // namespace client
} // namespace nx
