#include "action.h"

#include <QtCore/QCoreApplication>
#include <QtGui/QShortcutEvent>
#include <QtWidgets/QGraphicsWidget>

#include <utils/common/warnings.h>

#include <core/resource/layout_resource.h>
#include <core/resource/media_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_layout_snapshot_manager.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_display.h> // TODO: this one does not belong here.

#include <client/client_settings.h>

#include "action_manager.h"
#include "action_target_provider.h"
#include "action_conditions.h"
#include "action_factories.h"
#include "action_parameter_types.h"

QnAction::QnAction(Qn::ActionId id, QObject *parent): 
    QAction(parent), 
    QnWorkbenchContextAware(parent),
    m_id(id),
    m_flags(0),
    m_toolTipMarker(lit("<b></b>"))
{
    setToolTip(m_toolTipMarker);

    connect(this, &QAction::changed, this, &QnAction::updateToolTipSilent);
}

QnAction::~QnAction() {
    qDeleteAll(m_textConditions.uniqueKeys());
}


void QnAction::setRequiredPermissions(Qn::Permissions requiredPermissions) {
    setRequiredPermissions(-1, requiredPermissions);
}

void QnAction::setRequiredPermissions(int target, Qn::Permissions requiredPermissions) {
    m_permissions[target].required = requiredPermissions;
}

void QnAction::setForbiddenPermissions(int target, Qn::Permissions forbiddenPermissions) {
    m_permissions[target].forbidden = forbiddenPermissions;
}

void QnAction::setForbiddenPermissions(Qn::Permissions forbiddenPermissions) {
    setForbiddenPermissions(-1, forbiddenPermissions);
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
        disconnect(this, &QAction::toggled, this, &QnAction::updateText);
    } else {
        connect(this, &QAction::toggled, this, &QnAction::updateText, Qt::UniqueConnection);
    }

    updateText();
}

void QnAction::setPulledText(const QString &pulledText) {
    m_pulledText = pulledText;
}

void QnAction::setCondition(QnActionCondition *condition) {
    m_condition = condition;
}

void QnAction::setChildFactory(QnActionFactory *childFactory) {
    m_childFactory = childFactory;
}

void QnAction::addChild(QnAction *action) {
    m_children.push_back(action);
}

void QnAction::removeChild(QnAction *action) {
    m_children.removeOne(action);
}

QString QnAction::defaultToolTipFormat() const {
    if(shortcuts().empty()) {
        return tr("%n");
    } else {
        return tr("%n (<b>%s</b>)");
    }
}

QString QnAction::toolTipFormat() const {
    return m_toolTipFormat.isEmpty() ? defaultToolTipFormat() : m_toolTipFormat;
}

void QnAction::setToolTipFormat(const QString &toolTipFormat) {
    if(m_toolTipFormat == toolTipFormat)
        return;

    m_toolTipFormat = toolTipFormat;

    updateToolTip(true);
}

Qn::ActionVisibility QnAction::checkCondition(Qn::ActionScopes scope, const QnActionParameters &parameters) const {
    if(!isVisible())
        return Qn::InvisibleAction; // TODO: #Elric cheat!

    if(!(this->scope() & scope) && this->scope() != scope)
        return Qn::InvisibleAction;

    if((m_flags & Qn::DevMode) && !qnSettings->isDevMode())
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
        for(QHash<int, Permissions>::const_iterator pos = m_permissions.begin(), end = m_permissions.end(); pos != end; pos++) {
            int key = pos.key();
            Qn::Permissions required = pos->required;
            Qn::Permissions forbidden = pos->forbidden;

            QnResourceList resources;
            if(parameters.hasArgument(key)) {
                resources = QnActionParameterTypes::resources(parameters.argument(key));
            } else if(key == Qn::CurrentLayoutResourceRole) {
                if (QnLayoutResourcePtr layout = context()->workbench()->currentLayout()->resource())
                    resources.push_back(layout);
            } else if(key == Qn::CurrentUserResourceRole) {
                if (QnUserResourcePtr user = context()->user())
                    resources.push_back(user);
            } else if(key == Qn::CurrentMediaServerResourcesRole) {
                resources = context()->resourcePool()->getResources<QnMediaServerResource>();
            } else if(key == Qn::CurrentLayoutMediaItemsRole) {
                const QnResourceList& resList = QnActionParameterTypes::resources(context()->display()->widgets());
                foreach( QnResourcePtr res, resList )
                {
                    if( res.dynamicCast<QnMediaResource>() )
                        resources.push_back( res );
                }
            }

            if (resources.isEmpty() && required > 0)
                return Qn::InvisibleAction;
            if((accessController()->permissions(resources) & required) != required)
                return Qn::InvisibleAction;
            if((accessController()->permissions(resources) & forbidden) != 0)
                return Qn::InvisibleAction;
        }
    }

    if(m_condition) {
        if (parameters.scope() == Qn::InvalidScope) {
            QnActionParameters scopedParameters(parameters);
            scopedParameters.setScope(scope);
            return m_condition->check(scopedParameters);
        }
        return m_condition->check(parameters);
    }

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

        Qn::ActionScope scope = Qn::InvalidScope;
        QnActionParameters parameters;
        QnActionTargetProvider *targetProvider = QnWorkbenchContextAware::menu()->targetProvider();
        if(targetProvider != NULL) {
            if(flags() & Qn::ScopelessHotkey) {
                scope = static_cast<Qn::ActionScope>(static_cast<int>(this->scope()));
            } else {
                scope = targetProvider->currentScope();
            }

            if(flags() & Qn::TargetlessHotkey) {
                /* Parameters are empty. */
            } else {
                parameters = targetProvider->currentParameters(scope);
            }
        }

        if(checkCondition(scope, parameters) == Qn::EnabledAction)
            QnWorkbenchContextAware::menu()->trigger(id(), parameters);
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

void QnAction::updateToolTip(bool notify) {
    if(!toolTip().endsWith(m_toolTipMarker))
        return; /* We have an explicitly set tooltip. */

    /* This slot is the first to be invoked from changed() signal,
     * so we don't want to emit additional changed() signals if we were called from it. */
    bool signalsBlocked = false;
    if(notify)
        signalsBlocked = blockSignals(true);

    QString toolTip = toolTipFormat();

    int nameIndex = toolTip.indexOf(lit("%n"));
    if(nameIndex != -1) {
        QString name = !m_pulledText.isEmpty() ? m_pulledText : text();
        toolTip.replace(nameIndex, 2, name);
    }

    int shortcutIndex = toolTip.indexOf(lit("%s"));
    if(shortcutIndex != -1)
        toolTip.replace(shortcutIndex, 2, shortcut().toString(QKeySequence::NativeText));

    setToolTip(toolTip + m_toolTipMarker);

    if(notify)
        blockSignals(signalsBlocked);
}

void QnAction::updateToolTipSilent() {
    updateToolTip(false);
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
