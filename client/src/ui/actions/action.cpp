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
#include <ui/workbench/workbench_access_controller.h>

#include "action_manager.h"
#include "action_target_provider.h"
#include "action_conditions.h"
#include "action_parameter_types.h"


QnAction::QnAction(Qn::ActionId id, QObject *parent): 
    QAction(parent), 
    QnWorkbenchContextAware(parent),
    m_id(id),
    m_flags(0)
{}

QnAction::~QnAction() {}

void QnAction::setRequiredPermissions(Qn::Permissions requiredPermissions) {
    setRequiredPermissions(QString(), requiredPermissions);
}

void QnAction::setRequiredPermissions(const QString &target, Qn::Permissions requiredPermissions) {
    m_requiredPermissions[target] = requiredPermissions;
}

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

void QnAction::setPulledText(const QString &pulledText) {
    m_pulledText = pulledText;
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

Qn::ActionVisibility QnAction::checkCondition(Qn::ActionScopes scope, const QnActionParameters &parameters) const {
    if(!(this->scope() & scope) && scope != this->scope())
        return Qn::InvisibleAction;

    int size = parameters.size();

    if(size == 0 && !(m_flags & Qn::NoTarget))
        return Qn::InvisibleAction;

    if(size == 1 && !(m_flags & Qn::SingleTarget))
        return Qn::InvisibleAction;

    if(size > 1 && !(m_flags & Qn::MultiTarget))
        return Qn::InvisibleAction;

    Qn::ActionParameterType type = parameters.type();
    if(!(this->targetTypes() & type) && size != 0)
        return Qn::InvisibleAction;

    if(!m_requiredPermissions.empty()) {
        for(QHash<QString, Qn::Permissions>::const_iterator pos = m_requiredPermissions.begin(), end = m_requiredPermissions.end(); pos != end; pos++) {
            const QString &key = pos.key();
            Qn::Permissions requirements = pos.value();

            QnResourceList resources;
            if(key.isEmpty()) {
                resources = parameters.resources();
            } else if(parameters.hasArgument(key)) {
                resources = QnActionParameterTypes::resources(parameters.argument(key));
            } else if(key == Qn::CurrentLayoutParameter) {
                resources.push_back(context()->workbench()->currentLayout()->resource());
            } else if(key == Qn::CurrentUserParameter) {
                resources.push_back(context()->user());
            }

            if(resources.empty() && key.isEmpty()) {
                if((accessController()->permissions() & requirements) != requirements)
                    return Qn::InvisibleAction;
            } else {
                if((accessController()->permissions(resources) & requirements) != requirements)
                    return Qn::InvisibleAction;
            }
        }
    }

    if(m_condition)
        return m_condition.data()->check(parameters);

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

        QnActionParameters parameters;
        QnActionTargetProvider *targetProvider = QnWorkbenchContextAware::menu()->targetProvider();
        if(targetProvider != NULL) {
            if(flags() & Qn::ScopelessHotkey) {
                parameters.setItems(targetProvider->currentTarget(static_cast<Qn::ActionScope>(static_cast<int>(scope()))));
            } else {
                Qn::ActionScope scope = targetProvider->currentScope();
                if(this->scope() & scope)
                    parameters.setItems(targetProvider->currentTarget(scope));
            }
        }

        if(checkCondition(scope(), parameters) == Qn::EnabledAction)
            QnWorkbenchContextAware::menu()->trigger(m_id, parameters);
        return true;
    }
    
    return QObject::event(event);
}

void QnAction::at_toggled(bool checked) {
    setText(checked ? m_toggledText : m_normalText);
}

