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

    QMenu *newMenu(Qn::ActionScope scope);

    QMenu *newMenu(Qn::ActionScope scope, const QVariant &items);

    QMenu *newMenu(Qn::ActionScope scope, const QnResourceList &resources);

    QMenu *newMenu(Qn::ActionScope scope, const QList<QGraphicsItem *> &items);

    QMenu *newMenu(Qn::ActionScope scope, const QnResourceWidgetList &widgets);

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

    QnResourceWidgetList currentWidgetsTarget(QnAction *action) const;

    Qn::ActionTarget currentTargetType(QObject *sender) const;

    QVariant currentTarget(QObject *sender) const;

    QnResourceList currentResourcesTarget(QObject *sender) const;

    QnResourcePtr currentResourceTarget(QObject *sender) const;

    QnLayoutItemIndexList currentLayoutItemsTarget(QObject *sender) const;

    QnResourceWidgetList currentWidgetsTarget(QObject *sender) const;

protected:
    friend class QnAction;
    friend class QnActionFactory;

    QMenu *newMenuInternal(const QnAction *parent, Qn::ActionScope scope, const QVariant &items);

    QMenu *newMenuRecursive(const QnAction *parent, Qn::ActionScope scope, const QVariant &items);

private slots:
    void at_menu_destroyed(QObject *menu);

private:
    /** Mapping from action id to action data. */ 
    QHash<Qn::ActionId, QnAction *> m_actionById;

    /** Root action data. Also contained in the map above. */
    QnAction *m_root;

    /** Set of all menus created by this object that are not yet destroyed. */
    QSet<QObject *> m_menus;

    /** Target provider for actions. */
    QnActionTargetProvider *m_targetProvider;

    /** Guard for target provider. */
    QWeakPointer<QObject> m_targetProviderGuard;

    /** Currently active action that was activated via a shortcut. */
    QnAction *m_shortcutAction;

    /** List of items supplied to the last call to <tt>newMenu</tt>. */
    QVariant m_lastTarget;

};


#define qnMenu          (QnActionManager::instance())
#define qnAction(id)    (QnActionManager::instance()->action(id))

#endif // QN_CONTEXT_MENU_H
