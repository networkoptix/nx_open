#ifndef QN_ACTION_MANAGER_H
#define QN_ACTION_MANAGER_H

#include <QObject>
#include <QHash>
#include <QSet>
#include <QVariant>
#include <core/resource/resource_fwd.h>
#include <core/resource/layout_item_index.h>
#include <ui/workbench/workbench_context_aware.h>
#include "action_fwd.h"
#include "actions.h"
#include "action_parameters.h"

class QAction;
class QMenu;
class QGraphicsItem;

class QnActionFactory;
class QnActionBuilder;
class QnWorkbenchContext;

class QnActionManager: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnActionManager(QObject *parent = NULL);

    virtual ~QnActionManager();

    QAction *action(Qn::ActionId id) const;

    QList<QnAction *> actions() const;

    bool canTrigger(Qn::ActionId id, const QnActionParameters &parameters = QnActionParameters());

    void trigger(Qn::ActionId id, const QnActionParameters &parameters = QnActionParameters());

    QMenu *newMenu(Qn::ActionScope scope, const QnActionParameters &parameters = QnActionParameters());

    QnActionTargetProvider *targetProvider() const {
        return m_targetProviderGuard ? m_targetProvider : NULL;
    }

    void setTargetProvider(QnActionTargetProvider *targetProvider);

    QnActionParameters currentParameters(QnAction *action) const;

    QnActionParameters currentParameters(QObject *sender) const;

    void redirectAction(QMenu *menu, Qn::ActionId targetId, QAction *targetAction);

protected:
    friend class QnAction;
    friend class QnActionFactory;

    void copyAction(QAction *dst, QnAction *src, bool forwardSignals = true);

    QMenu *newMenuRecursive(const QnAction *parent, Qn::ActionScope scope, const QnActionParameters &parameters);

    bool redirectActionRecursive(QMenu *menu, Qn::ActionId targetId, QAction *targetAction);

private slots:
    void at_menu_destroyed(QObject *menu);
    void at_menu_aboutToShow();

private:
    /** Mapping from action id to action data. */ 
    QHash<Qn::ActionId, QnAction *> m_actionById;

    /** Mapping from action to action id. */
    QHash<QAction *, Qn::ActionId> m_idByAction;

    /** Root action data. Also contained in the map above. */
    QnAction *m_root;

    /** Mapping from a menu created by this manager to the parameters that were 
     * passed to it at construction time. NULL key is used for shortcut actions. */
    QHash<QObject *, QnActionParameters> m_parametersByMenu;

    /** Target provider for actions. */
    QnActionTargetProvider *m_targetProvider;

    /** Guard for target provider. */
    QWeakPointer<QObject> m_targetProviderGuard;

    /** Currently active action that was activated via a shortcut. */
    QnAction *m_shortcutAction;

    /** Last menu that was shown to the user. */
    QObject *m_lastShownMenu;
};


#endif // QN_ACTION_MANAGER_H
