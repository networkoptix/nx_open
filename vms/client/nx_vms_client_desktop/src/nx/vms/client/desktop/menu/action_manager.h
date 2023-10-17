// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QVariant>

#include <nx/vms/client/desktop/window_context_aware.h>

#include "action_fwd.h"
#include "action_parameters.h"
#include "actions.h"

namespace nx::vms::client::desktop {

class WindowContext;

namespace menu {

/**
 * Manager of menu actions. Creates all actions, which are visible in all context menus (and also
 * some other, which are used for convinience). Provides interface to send action from a single
 * source to any number of receivers using QAction mechanism. Implements complex way of condition
 * checking for those actions.
 */
class Manager: public QObject, public WindowContextAware
{
    Q_OBJECT;
public:
    enum CreationOption
    {
        DontReuseActions = 0x1
    };
    Q_DECLARE_FLAGS(CreationOptions, CreationOption)

    Manager(WindowContext* windowContext, QObject* parent = nullptr);
    virtual ~Manager() override;

    /**
     * Register a new action within this action manager.
     */
    void registerAction(Action* action);

    /**
     * Registers source action id as an alias for target action id.
     *
     * @param sourceId Alias id.
     * @param targetId Id of the target action.
     */
    void registerAlias(IDType sourceId, IDType targetId);

    /**
     * Action for the given id, or nullptr if no such action exists.
     */
    Action* action(IDType id) const;

    /**
     * List of all actions of this action manager.
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
    * Triggers the action with the given id without any checks.
    *
    * \param id                        Id of the action to trigger.
    * \param parameters                Parameters to pass to action handler.
    */
    void triggerForced(IDType id, const Parameters& parameters = Parameters());

    /**
     * \param scope                     Scope of the menu to create.
     * \param parameters                Action parameters for menu creation.
     * \param parent                    Parent widget for the new menu.
     * \param actionIds                 If not empty, only allow actions from the list.
     * \returns                         Newly created menu for the given parameters.
     *                                  Ownership of the created menu is passed to
     *                                  the caller.
     */
    QMenu* newMenu(
        ActionScope scope,
        QWidget* parent = nullptr,
        const Parameters& parameters = Parameters(),
        CreationOptions options = {},
        const QSet<IDType>& actionIds = {});

    QMenu* newMenu(
        IDType rootId,
        ActionScope scope,
        QWidget* parent = nullptr,
        const Parameters& parameters = Parameters(),
        CreationOptions options = {},
        const QSet<IDType>& actionIds = {});

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
        CreationOptions options,
        const QSet<IDType>& actionIds);

    bool redirectActionRecursive(QMenu* menu, IDType targetId, QAction* targetAction);

    /** Setup proper connections between menu and the manager. */
    QMenu* integrateMenu(QMenu* menu, const Parameters& parameters);

    /** Hide all menus that are currently opened. */
    void hideAllMenus();

private:
    void at_menu_destroyed(QObject* menuObj);

private:
    /** Root action. Also contained in the maps. */
    Action* m_root = nullptr;

    /** Mapping from action id to action instance. */
    QHash<IDType, Action*> m_actionById;

    /** Mapping from action to action id. */
    QHash<Action*, IDType> m_idByAction;

    /** Mapping from a menu created by this manager to the parameters that were
     * passed to it at construction time. nullptr key is used for shortcut actions. */
    QHash<QMenu*, Parameters> m_parametersByMenu;

    /** Target provider for actions. */
    TargetProvider* m_targetProvider = nullptr;

    /** Guard for target provider. */
    QPointer<QObject> m_targetProviderGuard;

    /** Currently active action that was activated via a shortcut. */
    Action* m_shortcutAction = nullptr;

    /** Last menu that was clicked by the user. */
    QMenu* m_lastClickedMenu = nullptr;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Manager::CreationOptions)

} // namespace menu
} // namespace nx::vms::client::desktop
