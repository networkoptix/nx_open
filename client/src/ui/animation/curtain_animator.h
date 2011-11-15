#ifndef QN_CURTAIN_ANIMATOR_H
#define QN_CURTAIN_ANIMATOR_H

#include <QObject>
#include <QWeakPointer>

class QParallelAnimationGroup;
class QPropertyAnimation;

class QnCurtainItem;
class QnDisplayWidget;

class QnCurtainAnimator: public QObject {
    Q_OBJECT;

public:
    QnCurtainAnimator(int durationMSec, QObject *parent = NULL);

    QnCurtainItem *curtainItem() const {
        return m_curtain;
    }

    void setCurtainItem(QnCurtainItem *curtain);

    void curtain(QnDisplayWidget *frontWidget = NULL);

    void uncurtain();

signals:
    void curtained();
    void uncurtained();

private slots:
    void at_curtain_destroyed();
    void at_animation_finished();

private:
    void restoreBorderColor();
    void initBorderColorAnimation(QObject *target);

    void restoreCurtainColor();
    void initCurtainColorAnimation();

private:
    QnCurtainItem *m_curtain;
    bool m_curtained;
    int m_durationMSec;
    QParallelAnimationGroup *m_animationGroup;
    QPropertyAnimation *m_curtainColorAnimation;
    QPropertyAnimation *m_borderColorAnimation;
};


#endif // QN_CURTAIN_ANIMATOR_H
