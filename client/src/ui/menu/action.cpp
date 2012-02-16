#include "action.h"
#include <QEvent>
#include <QShortcutEvent>
#include <utils/common/warnings.h>
#include "action_manager.h"
#include "action_target_provider.h"
#include "action_conditions.h"
#include "action_meta_types.h"

QnAction::QnAction(QnActionManager *manager, Qn::ActionId id, QObject *parent): 
    QAction(parent), 
    m_manager(manager),
    m_id(id),
    m_flags(0)
{}

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

bool QnAction::satisfiesCondition(Qn::ActionScope scope, const QVariant &items) const {
    if(!(m_flags & scope))
        return false;

    int size = QnActionMetaTypes::size(items);

    if(size == 0 && !(m_flags & Qn::NoTarget))
        return false;

    if(size == 1 && !(m_flags & Qn::SingleTarget))
        return false;

    if(size > 1 && !(m_flags & Qn::MultiTarget))
        return false;

    if(!m_condition.isNull() && !m_condition.data()->check(items))
        return false;

    return true;
}

bool QnAction::event(QEvent *event) {
    if (event->type() == QEvent::Shortcut) {
        QShortcutEvent *e = static_cast<QShortcutEvent *>(event);
        if (e->isAmbiguous()) {
            qnWarning("Ambiguous shortcut overload: %s.", e->key().toString());
        } else {
            QVariant target;
            QnActionTargetProvider *targetProvider = m_manager->targetProvider();
            if(targetProvider != NULL)
                target = targetProvider->target(this);

            if(satisfiesCondition(static_cast<Qn::ActionScope>(static_cast<int>(m_flags) & Qn::ScopeMask), target)) {
                m_manager->m_shortcutAction = this;
                m_manager->m_lastTarget = target;
                activate(Trigger);
                m_manager->m_shortcutAction = NULL;
            }
        }
        return true;
    }
    
    return QObject::event(event);
}

void QnAction::at_toggled(bool checked) {
    setText(checked ? m_normalText : m_toggledText);
}

