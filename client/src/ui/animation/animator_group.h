#ifndef QN_ANIMATOR_GROUP_H
#define QN_ANIMATOR_GROUP_H

#include <QAnimationGroup>

#include "abstract_animator.h"

class QnAnimatorGroup: public QnAbstractAnimator {
public:
    QnAnimatorGroup(QObject *parent = NULL);

    void addAnimator(QnAbstractAnimator *animator);
    
    QnAbstractAnimator *animatorAt(int index) const;

    int animatorCount() const;

    void clear()

    int indexOfAnimator(QnAbstractAnimator *animation) const

    void insertAnimator(int index, QnAbstractAnimator *animation)

    void removeAnimator(QnAbstractAnimator *animation)

    QnAbstractAnimator *takeAnimation(int index)

private:

};

#endif // QN_ANIMATOR_GROUP_H
