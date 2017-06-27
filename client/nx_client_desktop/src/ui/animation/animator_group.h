#ifndef QN_ANIMATOR_GROUP_H
#define QN_ANIMATOR_GROUP_H

#include "abstract_animator.h"

class AnimatorGroup: public AbstractAnimator {
    Q_OBJECT;

    typedef AbstractAnimator base_type;

public:
    AnimatorGroup(QObject *parent = NULL);

    virtual ~AnimatorGroup();

    void addAnimator(AbstractAnimator *animator);

    AbstractAnimator *animatorAt(int index) const;

    int animatorCount() const;

    void clear();

    int indexOfAnimator(AbstractAnimator *animator) const;

    void insertAnimator(int index, AbstractAnimator *animator);

    void removeAnimator(AbstractAnimator *animator);

    AbstractAnimator *takeAnimator(int index);

    QList<AbstractAnimator*> animators() const;
protected:
    virtual bool event(QEvent *event) override;

    virtual void updateState(State newState) override;

    virtual void updateCurrentTime(int currentTime) override;

    virtual int estimatedDuration() const override;

private:
    QList<AbstractAnimator *> m_animators;
};

#endif // QN_ANIMATOR_GROUP_H
