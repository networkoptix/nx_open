// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "animator_group.h"
#include <QtCore/QEvent>

#include <nx/utils/log/log.h>

AnimatorGroup::AnimatorGroup(QObject *parent):
    AbstractAnimator(parent)
{}

AnimatorGroup::~AnimatorGroup() {
    stop();
}


AbstractAnimator *AnimatorGroup::animatorAt(int index) const {
    if (index < 0 || index >= m_animators.size()) {
        NX_ASSERT(false, "Index is out of bounds.");
        return nullptr;
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
        NX_ASSERT(false, "Index is out of bounds.");
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
    if (!NX_ASSERT(animator))
        return;

    int index = m_animators.indexOf(animator);
    if (index == -1) {
        NX_ASSERT(false, "Given animator is not part of this group.");
        return;
    }

    takeAnimator(index);
}

AbstractAnimator *AnimatorGroup::takeAnimator(int index) {
    if (index < 0 || index >= m_animators.size()) {
        NX_ASSERT(false, "No animator at index %1.", index);
        return nullptr;
    }

    AbstractAnimator *animator = m_animators[index];
    animator->m_group = nullptr;
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
        if (animator != nullptr && animator->group() != this)
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
