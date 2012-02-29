#ifndef QN_CONTEXT_MENU_H
#define QN_CONTEXT_MENU_H

#include <QObject>
#include <QHash>
#include <QSet>
#include <QVariant>
#include <core/resource/resource_fwd.h>
#include <core/resource/layout_item_index.h>
#include "action_fwd.h"
#include "actions.h"

class QAction;
class QMenu;
class QGraphicsItem;

class QnActionFactory;
class QnActionBuilder;

class QnActionManager: public QObject {
    Q_OBJECT;
public:
    QnActionManager(QObject *parent = NULL);

    virtual ~QnActionManager();

    static QnActionManager *instance();

    QAction *action(Qn::ActionId id) const;

    QList<QnAction *> actions() const;

    void trigger(Qn::ActionId id);

    void trigger(Qn::ActionId id, const QVariant &items);

    void trigger(Qn::ActionId id, const QnResourcePtr &resource);

    void trigger(Qn::ActionId id, const QnResourceList &resources);

    void trigger(Qn::ActionId id, const QList<QGraphicsItem *> &items);

    void trigger(Qn::ActionId id, const QnResourceWidgetList &widgets);

    void trigger(Qn::ActionId id, const QnWorkbenchLayoutList &layouts);

    void trigger(Qn::ActionId id, const QnLayoutItemIndexList &layoutItems);

    QMenu *newMenu(Qn::ActionScope scope);

    QMenu *newMenu(Qn::ActionScope scope, const QVariant &items);

    QMenu *newMenu(Qn::ActionScope scope, const QnResourceList &resources);

    QMenu *newMenu(Qn::ActionScope scope, const QList<QGraphicsItem *> &items);

    QMenu *newMenu(Qn::ActionScope scope, const QnResourceWidgetList &widgets);

    QMenu *newMenu(Qn::ActionScope scope, const QnWorkbenchLayoutList &layouts);

    QMenu *newMenu(Qn::ActionScope scope, const QnLayoutItemIndexList &layoutItems);

    QnActionTargetProvider *targetProvider() const {
        return m_targetProviderGuard ? m_targetProvider : NULL;
    }

    void setTargetProvider(QnActionTargetProvider *targetProvider);

    Qn::ActionTarget currentTargetType(QnAction *action) const;

    QVariant currentTarget(QnAction *action) const;

    QnResourceList currentResourcesTarget(QnAction *action) const;

    QnResourcePtr currentResourceTarget(QnAction *action) const;

    QnLayoutItemIndexList currentLayoutItemsTarget(QnAction *action) const;

    QnWorkbenchLayoutList currentLayoutsTarget(QnAction *action) const;

    QnResourceWidgetList currentWidgetsTarget(QnAction *action) const;

    Qn::ActionTarget currentTargetType(QObject *sender) const;

    QVariant currentTarget(QObject *sender) const;

    QnResourceList currentResourcesTarget(QObject *sender) const;

    QnResourcePtr currentResourceTarget(QObject *sender) const;

    QnLayoutItemIndexList currentLayoutItemsTarget(QObject *sender) const;

    QnWorkbenchLayoutList currentLayoutsTarget(QObject *sender) const;

    QnResourceWidgetList currentWidgetsTarget(QObject *sender) const;

protected:
    friend class QnAction;
    friend class QnActionFactory;

    void triggerInternal(Qn::ActionId id, const QVariant &items);

    QMenu *newMenuInternal(const QnAction *parent, Qn::ActionScope scope, const QVariant &items);

    QMenu *newMenuRecursive(const QnAction *parent, Qn::ActionScope scope, const QVariant &items);

private slots:
    void at_menu_destroyed(QObject *menu);
    void at_menu_aboutToShow();

private:
    /** Mapping from action id to action data. */ 
    QHash<Qn::ActionId, QnAction *> m_actionById;

    /** Root action data. Also contained in the map above. */
    QnAction *m_root;

    /** Mapping from a menu created by this manager to the parameters that were 
     * passed to it at construction time. NULL key is used for shortcut actions. */
    QHash<QObject *, QVariant> m_targetByMenu;

    /** Target provider for actions. */
    QnActionTargetProvider *m_targetProvider;

    /** Guard for target provider. */
    QWeakPointer<QObject> m_targetProviderGuard;

    /** Currently active action that was activated via a shortcut. */
    QnAction *m_shortcutAction;

    /** Last menu that was shown to the user. */
    QObject *m_lastShownMenu;
};


#define qnMenu          (QnActionManager::instance())
#define qnAction(id)    (QnActionManager::instance()->action(id))

#endif // QN_CONTEXT_MENU_H
