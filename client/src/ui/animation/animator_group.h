#ifndef QN_ANIMATOR_GROUP_H
#define QN_ANIMATOR_GROUP_H

#include "abstract_animator.h"

class QnAnimatorGroup: public QnAbstractAnimator {
    Q_OBJECT;

    typedef QnAbstractAnimator base_type;

public:
    QnAnimatorGroup(QObject *parent = NULL);

    virtual ~QnAnimatorGroup();

    void addAnimator(QnAbstractAnimator *animator);
    
    QnAbstractAnimator *animatorAt(int index) const;

    int animatorCount() const;

    void clear();

    int indexOfAnimator(QnAbstractAnimator *animator) const;

    void insertAnimator(int index, QnAbstractAnimator *animator);

    void removeAnimator(QnAbstractAnimator *animator);

    QnAbstractAnimator *takeAnimator(int index);

protected:
    virtual bool event(QEvent *event) override;

    virtual void updateState(State newState) override;

    virtual void updateCurrentTime(int currentTime) override;

    virtual int estimatedDuration() const override;

private:
    QList<QnAbstractAnimator *> m_animators;
};

#endif // QN_ANIMATOR_GROUP_H
