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

    // TODO: to hell with all these overloads. Add QnActionParameters class.

    bool canTrigger(Qn::ActionId id, const QnResourcePtr &resource, const QVariantMap &params = QVariantMap());

    void trigger(Qn::ActionId id, const QVariantMap &params = QVariantMap());

    void trigger(Qn::ActionId id, const QVariant &items, const QVariantMap &params = QVariantMap());

    void trigger(Qn::ActionId id, const QnResourcePtr &resource, const QVariantMap &params = QVariantMap());

    void trigger(Qn::ActionId id, const QnResourceList &resources, const QVariantMap &params = QVariantMap());

    void trigger(Qn::ActionId id, const QList<QGraphicsItem *> &items, const QVariantMap &params = QVariantMap());

    void trigger(Qn::ActionId id, const QnResourceWidgetList &widgets, const QVariantMap &params = QVariantMap());

    void trigger(Qn::ActionId id, const QnWorkbenchLayoutList &layouts, const QVariantMap &params = QVariantMap());

    void trigger(Qn::ActionId id, const QnLayoutItemIndexList &layoutItems, const QVariantMap &params = QVariantMap());

    QMenu *newMenu(Qn::ActionScope scope, const QVariantMap &params = QVariantMap());

    QMenu *newMenu(Qn::ActionScope scope, const QVariant &items, const QVariantMap &params = QVariantMap());

    QMenu *newMenu(Qn::ActionScope scope, const QnResourceList &resources, const QVariantMap &params = QVariantMap());

    QMenu *newMenu(Qn::ActionScope scope, const QList<QGraphicsItem *> &items, const QVariantMap &params = QVariantMap());

    QMenu *newMenu(Qn::ActionScope scope, const QnResourceWidgetList &widgets, const QVariantMap &params = QVariantMap());

    QMenu *newMenu(Qn::ActionScope scope, const QnWorkbenchLayoutList &layouts, const QVariantMap &params = QVariantMap());

    QMenu *newMenu(Qn::ActionScope scope, const QnLayoutItemIndexList &layoutItems, const QVariantMap &params = QVariantMap());

    QnActionTargetProvider *targetProvider() const {
        return m_targetProviderGuard ? m_targetProvider : NULL;
    }

    void setTargetProvider(QnActionTargetProvider *targetProvider);

    Qn::ActionTargetType currentTargetType(QnAction *action) const;

    QVariantMap currentParameters(QnAction *action) const;

    QVariant currentParameter(QnAction *action, const QString &name) const;

    QVariant currentTarget(QnAction *action) const;

    QnResourceList currentResourcesTarget(QnAction *action) const;

    QnResourcePtr currentResourceTarget(QnAction *action) const;

    QnLayoutItemIndexList currentLayoutItemsTarget(QnAction *action) const;

    QnWorkbenchLayoutList currentLayoutsTarget(QnAction *action) const;

    QnResourceWidgetList currentWidgetsTarget(QnAction *action) const;

    Qn::ActionTargetType currentTargetType(QObject *sender) const;

    QVariantMap currentParameters(QObject *sender) const;

    QVariant currentParameter(QObject *sender, const QString &name) const;

    QVariant currentTarget(QObject *sender) const;

    QnResourceList currentResourcesTarget(QObject *sender) const;

    QnResourcePtr currentResourceTarget(QObject *sender) const;

    QnLayoutItemIndexList currentLayoutItemsTarget(QObject *sender) const;

    QnWorkbenchLayoutList currentLayoutsTarget(QObject *sender) const;

    QnResourceWidgetList currentWidgetsTarget(QObject *sender) const;

    void redirectAction(QMenu *menu, Qn::ActionId targetId, QAction *targetAction);

protected:
    friend class QnAction;
    friend class QnActionFactory;

    struct ActionParameters {
        ActionParameters(QVariant items, QVariantMap params): items(items), params(params) {}
        ActionParameters() {}

        QVariant items;
        QVariantMap params;
    };

    void copyAction(QAction *dst, QnAction *src, bool forwardSignals = true);

    void triggerInternal(Qn::ActionId id, const QVariant &items, const QVariantMap &params);

    QMenu *newMenuInternal(const QnAction *parent, Qn::ActionScope scope, const QVariant &items, const QVariantMap &params);

    QMenu *newMenuRecursive(const QnAction *parent, Qn::ActionScope scope, const QVariant &items);

    bool redirectActionRecursive(QMenu *menu, Qn::ActionId targetId, QAction *targetAction);

    ActionParameters currentParametersInternal(QnAction *action) const;

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
    QHash<QObject *, ActionParameters> m_parametersByMenu;

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
