#include "action.h"

#include <QtCore/QCoreApplication>
#include <QtGui/QShortcutEvent>
#include <QtGui/QGraphicsWidget>

#include <utils/common/warnings.h>
#include <core/resource/user_resource.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/workbench.h>

#include "action_manager.h"
#include "action_target_provider.h"
#include "action_conditions.h"
#include "action_target_types.h"

QnAction::QnAction(QnActionManager *manager, Qn::ActionId id, QObject *parent): 
    QAction(parent), 
    m_manager(manager),
    m_id(id),
    m_flags(0)
{
    if(!manager)
        qnNullCritical(manager);
}

QnAction::~QnAction() {}

void QnAction::setFlags(Qn::ActionFlags flags) {
    m_flags = flags;
}

void QnAction::setNormalText(const QString &normalText) {
    m_normalText = normalText;
}

void QnAction::setToggledText(const QString &toggledText) {
    m_toggledText = toggledText;

    connect(this, SIGNAL(toggled(bool)), this, SLOT(at_toggled(bool)), Qt::UniqueConnection);
}

void QnAction::setCondition(QnActionCondition *condition) {
    m_condition = condition;
}

void QnAction::addChild(QnAction *action) {
    m_children.push_back(action);
}

void QnAction::removeChild(QnAction *action) {
    m_children.removeOne(action);
}

Qn::ActionVisibility QnAction::checkCondition(Qn::ActionScope scope, const QVariant &items, const QVariantMap &params) const {
    if(!(this->scope() & scope) && scope != this->scope())
        return Qn::InvisibleAction;

    int size = QnActionTargetTypes::size(items);

    if(size == 0 && !(m_flags & Qn::NoTarget))
        return Qn::InvisibleAction;

    if(size == 1 && !(m_flags & Qn::SingleTarget))
        return Qn::InvisibleAction;

    if(size > 1 && !(m_flags & Qn::MultiTarget))
        return Qn::InvisibleAction;

    Qn::ActionTarget target = QnActionTargetTypes::target(items);
    if(!(this->target() & target) && size != 0)
        return Qn::InvisibleAction;

    if(requiredAccessRights()) {
        QnUserResourcePtr user = m_manager->context()->user();
        bool isAdmin = user && user->isAdmin();

        if((requiredAccessRights() & Qn::AdminAccessRights) && !isAdmin)
            return Qn::InvisibleAction;

        if((requiredAccessRights() & Qn::EditLayout) && !isAdmin) {
            QnLayoutResourcePtr layout = params.value(Qn::LayoutParameter).value<QnLayoutResourcePtr>();
            if(!layout)
                layout = m_manager->context()->workbench()->currentLayout()->resource();

            if(!m_manager->context()->snapshotManager()->isLocal(layout))
                return Qn::InvisibleAction;
        }
    }

    if(m_condition)
        return m_condition.data()->check(items);

    return Qn::EnabledAction;
}

bool QnAction::event(QEvent *event) {
    if (event->type() == QEvent::Shortcut) {
        QShortcutEvent *e = static_cast<QShortcutEvent *>(event);
        if (e->isAmbiguous()) {
            if(m_flags & Qn::IntentionallyAmbiguous) {
                QSet<QAction *> actions;

                foreach(QWidget *widget, associatedWidgets())
                    foreach(QAction *action, widget->actions())
                        if(action->shortcuts().contains(e->key()))
                            actions.insert(action);

                foreach(QGraphicsWidget *widget, associatedGraphicsWidgets())
                    foreach(QAction *action, widget->actions())
                        if(action->shortcuts().contains(e->key()))
                            actions.insert(action);

                actions.remove(this);

                foreach(QAction *action, actions) {
                    QShortcutEvent se(e->key(), e->shortcutId(), false);
                    QCoreApplication::sendEvent(action, &se);
                }
            } else {
                qnWarning("Ambiguous shortcut overload: %1.", e->key().toString());
                return true;
            }
        } 

        QVariant target;
        QnActionTargetProvider *targetProvider = m_manager->targetProvider();
        if(targetProvider != NULL) {
            if(flags() & Qn::ScopelessHotkey) {
                target = targetProvider->currentTarget(scope());
            } else {
                Qn::ActionScope scope = targetProvider->currentScope();
                if(this->scope() & scope)
                    target = targetProvider->currentTarget(scope);
            }
        }



        if(checkCondition(scope(), target, QVariantMap()) == Qn::EnabledAction)
            m_manager->trigger(m_id, target);
        return true;
    }
    
    return QObject::event(event);
}

void QnAction::at_toggled(bool checked) {
    setText(checked ? m_toggledText : m_normalText);
}

