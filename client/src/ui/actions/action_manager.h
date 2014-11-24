#ifndef QN_ACTION_MANAGER_H
#define QN_ACTION_MANAGER_H

#include <QtCore/QObject>
#include <QHash>
#include <QtCore/QSet>
#include <QtCore/QVariant>
#include <QtCore/QPointer>
#include <core/resource/resource_fwd.h>
#include <core/resource/layout_item_index.h>
#include <ui/workbench/workbench_context_aware.h>
#include "action_fwd.h"
#include "actions.h"
#include "action_parameters.h"

class QAction;
class QMenu;
class QGraphicsItem;

class QnMenuFactory;
class QnActionBuilder;
class QnWorkbenchContext;

/**
 * Action manager stores application's actions and presents an interface for
 * creating context menus given current action scope and parameters.
 */
class QnActionManager: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    enum CreationOption {
        DontReuseActions = 0x1
    };
    Q_DECLARE_FLAGS(CreationOptions, CreationOption)

    /**
     * Default constructor.
     * 
     * \param parent                    Context-aware parent of this action manager.
     */
    QnActionManager(QObject *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~QnActionManager();

    /**
     * \param action                    New action to register with this action manager.
     */
    void registerAction(QnAction *action);

    /**
     * Registers action id as an alias for another action id. 
     * 
     * \param sourceId                  Alias id.
     * \param targetId                  Id of the target action.
     */
    void registerAlias(Qn::ActionId id, Qn::ActionId targetId);

    /**
     * \param id                        Id of the action to get.
     * \returns                         Action for the given id, or NULL if no such action exists.
     */
    QnAction *action(Qn::ActionId id) const;

    /**
     * \returns                         List of all actions of this action manager.
     */
    QList<QnAction *> actions() const;

    /**
     * \param id                        Id of the action to check.
     * \param parameters                Parameters to check.
     * \returns                         Whether the action with the given id can
     *                                  be triggered with supplied parameters.
     */
    bool canTrigger(Qn::ActionId id, const QnActionParameters &parameters = QnActionParameters());

    /**
     * Triggers the action with the given id.
     * 
     * \param id                        Id of the action to trigger.
     * \param parameters                Parameters to pass to action handler.
     */
    void trigger(Qn::ActionId id, const QnActionParameters &parameters = QnActionParameters());


    /**
     * Triggers the action with the given id if possible
     * 
     * \param id                        Id of the action to trigger.
     * \param parameters                Parameters to pass to action handler.
     * \returns                         Was action triggered or not.
     */
    bool triggerIfPossible(Qn::ActionId id, const QnActionParameters &parameters = QnActionParameters());

    /**
     * \param scope                     Scope of the menu to create.
     * \param parameters                Action parameters for menu creation.
     * \param parent                    Parent widget for the new menu.
     * \returns                         Newly created menu for the given paramters.
     *                                  Ownership of the created menu is passed to
     *                                  the caller.
     */
    QMenu *newMenu(Qn::ActionScope scope, QWidget *parent = NULL, const QnActionParameters &parameters = QnActionParameters(), CreationOptions options = 0);

    QMenu *newMenu(Qn::ActionId rootId, Qn::ActionScope scope, QWidget *parent  = NULL, const QnActionParameters &parameters = QnActionParameters(), CreationOptions options = 0);

    /**
     * \returns                         Action target provider that is assigned to this
     *                                  action manager.
     */
    QnActionTargetProvider *targetProvider() const {
        return m_targetProviderGuard ? m_targetProvider : NULL;
    }

    /**
     * \param targetProvider            New target provider for this action manager.
     */
    void setTargetProvider(QnActionTargetProvider *targetProvider);

    /**
     * \param action                    Action that has just been activated.
     * \returns                         Parameters with which the given action
     *                                  was triggered.
     */
    QnActionParameters currentParameters(QnAction *action) const;

    /**
     * This is a convenience overload to be used inside handlers of <tt>QAction</tt>'s
     * <tt>triggered()</tt> signal.
     *
     * \param sender                    Action that has just been activated.
     * \returns                         Parameters with which the given action
     *                                  was triggered.
     */
    QnActionParameters currentParameters(QObject *sender) const;

    /**
     * This function replaces one action in the given menu with another one.
     * 
     * \param menu                      Menu to replace the action in.
     * \param targetId                  Id of the action to be replaced.
     * \param targetAction              Replacement action.
     */
    void redirectAction(QMenu *menu, Qn::ActionId sourceId, QAction *targetAction);

    virtual bool eventFilter(QObject *watched, QEvent *event) override;

signals:
    void menuAboutToShow(QMenu* menu);
    void menuAboutToHide(QMenu* menu);

protected:
    friend class QnAction;

    void copyAction(QAction *dst, QnAction *src, bool forwardSignals = true);

    QMenu *newMenuRecursive(const QnAction *parent, Qn::ActionScope scope, const QnActionParameters &parameters, QWidget *parentWidget, CreationOptions options);

    bool redirectActionRecursive(QMenu *menu, Qn::ActionId targetId, QAction *targetAction);

    /** Setup proper connections between menu and the manager. */
    QMenu* integrateMenu(QMenu *menu, const QnActionParameters &parameters);
private slots:
    void at_menu_destroyed(QObject *menu);

private:
    /** Root action. Also contained in the maps. */
    QnAction *m_root;

    /** Mapping from action id to action instance. */ 
    QHash<Qn::ActionId, QnAction *> m_actionById;

    /** Mapping from action to action id. */
    QHash<QnAction *, Qn::ActionId> m_idByAction;

    /** Mapping from a menu created by this manager to the parameters that were 
     * passed to it at construction time. NULL key is used for shortcut actions. */
    QHash<QObject *, QnActionParameters> m_parametersByMenu;

    /** Target provider for actions. */
    QnActionTargetProvider *m_targetProvider;

    /** Guard for target provider. */
    QPointer<QObject> m_targetProviderGuard;

    /** Currently active action that was activated via a shortcut. */
    QnAction *m_shortcutAction;

    /** Last menu that was clicked by the user. */
    QObject *m_lastClickedMenu;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnActionManager::CreationOptions)


#endif // QN_ACTION_MANAGER_H
