#include "animator_group.h"
#include <QEvent>
#include <utils/common/warnings.h>

QnAnimatorGroup::QnAnimatorGroup(QObject *parent):
    QnAbstractAnimator(parent)
{}

QnAnimatorGroup::~QnAnimatorGroup() {
    stop();
}

QnAbstractAnimator *QnAnimatorGroup::animatorAt(int index) const {
    if (index < 0 || index >= m_animators.size()) {
        qnWarning("Index is out of bounds.");
        return NULL;
    }

    return m_animators[index];
}

int QnAnimatorGroup::animatorCount() const {
    return m_animators.size();
}

int QnAnimatorGroup::indexOfAnimator(QnAbstractAnimator *animator) const {
    return m_animators.indexOf(animator);
}

void QnAnimatorGroup::addAnimator(QnAbstractAnimator *animator) {
    insertAnimator(m_animators.size(), animator);
}

void QnAnimatorGroup::insertAnimator(int index, QnAbstractAnimator *animator) {
    if (index < 0 || index > m_animators.size()) {
        qnWarning("Index is out of bounds.");
        return;
    }

    if (QnAnimatorGroup *oldGroup = animator->group())
        oldGroup->removeAnimator(animator);

    m_animators.insert(index, animator);
    animator->m_group = this;
    animator->setParent(this); /* This will make sure that ChildAdded event is sent to 'this'. */
}

void QnAnimatorGroup::removeAnimator(QnAbstractAnimator *animator) {
    if (animator == NULL) {
        qnNullWarning(animator);
        return;
    }

    int index = m_animators.indexOf(animator);
    if (index == -1) {
        qnWarning("Given animator is not part of this group.");
        return;
    }

    takeAnimator(index);
}

QnAbstractAnimator *QnAnimatorGroup::takeAnimator(int index) {
    if (index < 0 || index >= m_animators.size()) {
        qnWarning("No animator at index %1.", index);
        return NULL;
    }

    QnAbstractAnimator *animator = m_animators[index];
    animator->m_group = NULL;
    m_animators.removeAt(index);
    
    /* Removing from list before doing setParent to avoid infinite recursion in ChildRemoved event. */
    animator->setParent(0); 

    if (m_animators.isEmpty())
        stop();

    return animator;
}

void QnAnimatorGroup::clear() {
    qDeleteAll(m_animators);
}

bool QnAnimatorGroup::event(QEvent *event) {
    if (event->type() == QEvent::ChildAdded) {
        QnAbstractAnimator *animator = qobject_cast<QnAbstractAnimator *>(static_cast<QChildEvent *>(event)->child());
        if (animator != NULL && animator->group() != this)
                addAnimator(animator);
    } else if (event->type() == QEvent::ChildRemoved) {
        /* We can only rely on the child being a QObject because in the QEvent::ChildRemoved case it might be called from the destructor. */
        QnAbstractAnimator *animator = static_cast<QnAbstractAnimator *>(static_cast<QChildEvent *>(event)->child());
        int index = m_animators.indexOf(animator);
        if (index != -1)
            takeAnimator(index);
    }

    return QnAbstractAnimator::event(event);
}

void QnAnimatorGroup::updateState(State newState) {
    if(newState == RUNNING)
        foreach(QnAbstractAnimator *animator, m_animators)
            animator->setDurationOverride(duration());

    foreach(QnAbstractAnimator *animator, m_animators)
        animator->setState(newState);

    base_type::updateState(newState);
}

void QnAnimatorGroup::updateCurrentTime(int deltaTime) {
    foreach(QnAbstractAnimator *animator, m_animators)
        animator->updateCurrentTime(deltaTime);
}

int QnAnimatorGroup::estimatedDuration() const {
    int result = 0;
    foreach(QnAbstractAnimator *animator, m_animators)
        result = qMax(result, animator->estimatedDuration());
    return result;
}

