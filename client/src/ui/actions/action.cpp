#include "action.h"

#include <QtCore/QCoreApplication>
#include <QtGui/QShortcutEvent>
#include <QtGui/QGraphicsWidget>

#include <utils/common/warnings.h>
#include <core/resource/user_resource.h>
#include <core/resourcemanagment/resource_pool.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_access_controller.h>

#include "action_manager.h"
#include "action_target_provider.h"
#include "action_conditions.h"
#include "action_parameter_types.h"

namespace {
    const char *const toolTipMarker = "<b></b>";
}

QnAction::QnAction(Qn::ActionId id, QObject *parent): 
    QAction(parent), 
    QnWorkbenchContextAware(parent),
    m_id(id),
    m_flags(0)
{
    setToolTip(QLatin1String(toolTipMarker));

    connect(this, SIGNAL(changed()), this, SLOT(updateToolTip()));
}

QnAction::~QnAction() {
    qDeleteAll(m_textConditions.uniqueKeys());
}


void QnAction::setRequiredPermissions(Qn::Permissions requiredPermissions) {
    setRequiredPermissions(QString(), requiredPermissions);
}

void QnAction::setRequiredPermissions(const QString &target, Qn::Permissions requiredPermissions) {
    m_permissions[target].required = requiredPermissions;
}

void QnAction::setForbiddenPermissions(const QString &target, Qn::Permissions forbiddenPermissions) {
    m_permissions[target].forbidden = forbiddenPermissions;
}

void QnAction::setForbiddenPermissions(Qn::Permissions forbiddenPermissions) {
    setForbiddenPermissions(QString(), forbiddenPermissions);
}

void QnAction::setFlags(Qn::ActionFlags flags) {
    m_flags = flags;
}

void QnAction::setNormalText(const QString &normalText) {
    if(m_normalText == normalText)
        return;

    m_normalText = normalText;

    updateText();
}

void QnAction::setToggledText(const QString &toggledText) {
    if(m_toggledText == toggledText)
        return;

    m_toggledText = toggledText;

    if(m_toggledText.isEmpty()) {
        disconnect(this, SIGNAL(toggled(bool)), this, SLOT(updateText()));
    } else {
        connect(this, SIGNAL(toggled(bool)), this, SLOT(updateText()), Qt::UniqueConnection);
    }

    updateText();
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
    if(!(this->defaultParameterTypes() & type) && size != 0)
        return Qn::InvisibleAction;

    if(!m_permissions.empty()) {
        for(QHash<QString, Permissions>::const_iterator pos = m_permissions.begin(), end = m_permissions.end(); pos != end; pos++) {
            const QString &key = pos.key();
            Qn::Permissions required = pos->required;
            Qn::Permissions forbidden = pos->forbidden;

            QnResourceList resources;
            if(parameters.hasArgument(key)) {
                resources = QnActionParameterTypes::resources(parameters.argument(key));
            } else if(key == Qn::CurrentLayoutParameter) {
                resources.push_back(context()->workbench()->currentLayout()->resource());
            } else if(key == Qn::CurrentUserParameter) {
                resources.push_back(context()->user());
            } else if(key == Qn::AllVideoServersParameter) {
                resources = context()->resourcePool()->getResources().filtered<QnVideoServerResource>();
            }

            if((accessController()->permissions(resources) & required) != required)
                return Qn::InvisibleAction;
            if((accessController()->permissions(resources) & forbidden) != 0)
                return Qn::InvisibleAction;
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

void QnAction::updateText() {
    if(isChecked()) {
        setText(toggledText());
    } else {
        setText(normalText());
    }
}

void QnAction::updateToolTip() {
    if(!toolTip().endsWith(QLatin1String(toolTipMarker)))
        return; /* We have an explicitly set tooltip. */

    /* This is the first handler in a chain, 
     * so we don't need to emit additional changed signal. */
    bool signalsBlocked = blockSignals(true);

    if(shortcuts().isEmpty()) {
        setToolTip(tr("%1%2").arg(text()).arg(QLatin1String(toolTipMarker))); 
    } else {
        setToolTip(tr("%1 (<b>%2</b>)%3").arg(text()).arg(shortcut().toString(QKeySequence::NativeText)).arg(QLatin1String(toolTipMarker)));
    }

    blockSignals(signalsBlocked);
}

void QnAction::addConditionalText(QnActionCondition *condition, const QString &text) {
    m_textConditions[condition] = text;
}

bool QnAction::hasConditionalTexts() {
    return !m_textConditions.isEmpty();
}

QString QnAction::checkConditionalText(const QnActionParameters &parameters) const {
    foreach (QnActionCondition *condition, m_textConditions.uniqueKeys()){
        if (condition->check(parameters))
            return m_textConditions[condition];
    }
    return normalText();
}
