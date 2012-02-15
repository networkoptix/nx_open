#include "action.h"
#include <QEvent>
#include <QShortcutEvent>
#include <utils/common/warnings.h>

QnAction::QnAction(Qn::ActionId id, QObject *parent): 
    QAction(parent), 
    m_id(id),
    m_flags(0),
    m_condition(NULL)
{}

QnAction::~QnAction() {}

void QnAction::setFlags(Qn::ActionFlags flags) {
    m_flags = flags;

    setSeparator(m_flags & Qn::Separator);
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

bool QnAction::satisfiesCondition(Qn::ActionScope scope) {
    return satisfiesConditionInternal(scope, QnResourceList());
}

bool QnAction::satisfiesCondition(Qn::ActionScope scope, const QnResourceList &resources) {
    return satisfiesConditionInternal(scope, resources);
}

bool QnAction::satisfiesCondition(Qn::ActionScope scope, const QList<QGraphicsItem *> &items) {
    return satisfiesConditionInternal(scope, items);
}

template<class ItemSequence>
bool QnAction::satisfiesConditionInternal(Qn::ActionScope scope, const ItemSequence &items) const {
    if(!(m_flags & scope))
        return false;

    if(items.size() == 0 && !(m_flags & NoTarget))
        return false;

    if(items.size() == 1 && !(m_flags & SingleTarget))
        return false;

    if(items.size() > 1 && !(m_flags & MultiTarget))
        return false;

    if(m_condition != NULL && !m_condition->check(items))
        return false;

    return true;
}

bool QnAction::event(QEvent *event) {
    if(m_data == NULL) {
        return QAction::event(event);
    } else {
        if (event->type() == QEvent::Shortcut) {
            QShortcutEvent *e = static_cast<QShortcutEvent *>(event);
            if (e->isAmbiguous()) {
                qnWarning("Ambiguous shortcut overload: %s.", e->key().toString());
            } else {


                activate(Trigger);
            }
            return true;
        }
        return QObject::event(e);
    }

    }
}

void QnAction::at_toggled(bool checked) {
    if(m_flags & ToggledText)
        setText(checked ? m_normalText : m_toggledText);
}

