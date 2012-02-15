#ifndef QN_CONTEXT_MENU_H
#define QN_CONTEXT_MENU_H

#include <QObject>
#include <QHash>
#include <QVariant>
#include <core/resource/resource_fwd.h>
#include "actions.h"

class QAction;
class QMenu;
class QGraphicsItem;

class QnActionData;
class QnActionFactory;
class QnActionBuilder;
class QnActionCondition;
class QnActionSourceProvider;

class QnContextMenu: public QObject {
    Q_OBJECT;
public:
    QnContextMenu(QObject *parent = NULL);

    virtual ~QnContextMenu();

    static QnContextMenu *instance();

    QAction *action(Qn::ActionId id) const;

    QMenu *newMenu(Qn::ActionScope scope);

    QMenu *newMenu(Qn::ActionScope scope, const QnResourceList &resources);

    QMenu *newMenu(Qn::ActionScope scope, const QList<QGraphicsItem *> &items);

    QnResourceList cause(QObject *sender);

    QnActionSourceProvider *sourceProvider() const {
        return m_sourceProviderGuard ? m_sourceProvider : NULL;
    }

    void setSourceProvider(QnActionSourceProvider *sourceProvider);

protected:
    friend class QnActionFactory;

    template<class ItemSequence>
    QMenu *newMenuInternal(const QnActionData *parent, Qn::ActionScope scope, const ItemSequence &items);

    template<class ItemSequence>
    QMenu *newMenuRecursive(const QnActionData *parent, Qn::ActionScope scope, const ItemSequence &items);

private slots:
    void at_action_toggled();
    void at_menu_destroyed(QObject *menu);

private:
    /** Mapping from action id to action data. */ 
    QHash<Qn::ActionId, QnActionData *> m_dataById;

    /** Root action data. Also contained in the map above. */
    QnActionData *m_root;

    /** Set of all menus created by this object that are not yet destroyed. */
    QSet<QObject *> m_menus;

    /** List of items supplied to the last call to <tt>newMenu</tt>. */
    QVariant m_lastQuery;

    /** Source provider for actions. */
    QnActionSourceProvider *m_sourceProvider;

    /** Guard for source provider. */
    QWeakPointer<QObject> m_sourceProviderGuard;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnContextMenu::ActionFlags);


#define qnMenu          (QnContextMenu::instance())
#define qnAction(id)    (QnContextMenu::instance()->action(id))

#endif // QN_CONTEXT_MENU_H
