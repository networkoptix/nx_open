#include "animator_group.h"
#include <QtCore/QEvent>
#include <utils/common/warnings.h>

AnimatorGroup::AnimatorGroup(QObject *parent):
    AbstractAnimator(parent)
{}

AnimatorGroup::~AnimatorGroup() {
    stop();
}


AbstractAnimator *AnimatorGroup::animatorAt(int index) const {
    if (index < 0 || index >= m_animators.size()) {
        qnWarning("Index is out of bounds.");
        return NULL;
    }

    return m_animators[index];
}

int AnimatorGroup::animatorCount() const {
    return m_animators.size();
}

int AnimatorGroup::indexOfAnimator(AbstractAnimator *animator) const {
    return m_animators.indexOf(animator);
}

void AnimatorGroup::addAnimator(AbstractAnimator *animator) {
    insertAnimator(m_animators.size(), animator);
}

void AnimatorGroup::insertAnimator(int index, AbstractAnimator *animator) {
    if (index < 0 || index > m_animators.size()) {
        qnWarning("Index is out of bounds.");
        return;
    }

    if (AnimatorGroup *oldGroup = animator->group())
        oldGroup->removeAnimator(animator);

    m_animators.insert(index, animator);
    animator->m_group = this;
    animator->setParent(this); /* This will make sure that ChildAdded event is sent to 'this'. */

    invalidateDuration();
}

void AnimatorGroup::removeAnimator(AbstractAnimator *animator) {
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

AbstractAnimator *AnimatorGroup::takeAnimator(int index) {
    if (index < 0 || index >= m_animators.size()) {
        qnWarning("No animator at index %1.", index);
        return NULL;
    }

    AbstractAnimator *animator = m_animators[index];
    animator->m_group = NULL;
    m_animators.removeAt(index);

    /* Removing from list before doing setParent to avoid infinite recursion in ChildRemoved event. */
    animator->setParent(0);

    if (m_animators.isEmpty())
        stop();

    invalidateDuration();

    return animator;
}

QList<AbstractAnimator*> AnimatorGroup::animators() const
{
    return m_animators;
}

void AnimatorGroup::clear()
{
    qDeleteAll(m_animators);
}

bool AnimatorGroup::event(QEvent *event) {
    if (event->type() == QEvent::ChildAdded) {
        AbstractAnimator *animator = qobject_cast<AbstractAnimator *>(static_cast<QChildEvent *>(event)->child());
        if (animator != NULL && animator->group() != this)
            addAnimator(animator);
    } else if (event->type() == QEvent::ChildRemoved) {
        /* We can only rely on the child being a QObject because in the QEvent::ChildRemoved case it might be called from the destructor. */
        AbstractAnimator *animator = static_cast<AbstractAnimator *>(static_cast<QChildEvent *>(event)->child());
        int index = m_animators.indexOf(animator);
        if (index != -1)
            takeAnimator(index);
    }

    return AbstractAnimator::event(event);
}

void AnimatorGroup::updateState(State newState) {
    if(newState == Running) {
        invalidateDuration();

        foreach(AbstractAnimator *animator, m_animators)
            animator->setDurationOverride(duration());
    }

    foreach(AbstractAnimator *animator, m_animators)
        animator->setState(newState);

    base_type::updateState(newState);
}

void AnimatorGroup::updateCurrentTime(int currentTime) {
    foreach(AbstractAnimator *animator, m_animators)
        animator->setCurrentTime(currentTime);
}

int AnimatorGroup::estimatedDuration() const
{
    static const auto getDuration =
        [](AbstractAnimator* animator)
        {
            const auto timeLimit = animator->timeLimit();
            const auto estimated = animator->estimatedDuration();
            return timeLimit >= 0 ? qMin(estimated, timeLimit) : estimated;
        };

    int result = 0;
    for (const auto animator: m_animators)
        result = qMax(result, getDuration(animator));

    return result;
}

