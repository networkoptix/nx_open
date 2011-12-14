#ifndef QN_CURTAIN_ANIMATOR_H
#define QN_CURTAIN_ANIMATOR_H

#include <QObject>
#include <QWeakPointer>

class AnimationTimer;

class QnCurtainItem;
class QnResourceWidget;
class QnVariantAnimator;
class QnAnimatorGroup;

class QnCurtainAnimator: public QObject {
    Q_OBJECT;

public:
    QnCurtainAnimator(int durationMSec, AnimationTimer *timer, QObject *parent = NULL);

    QnCurtainItem *curtainItem() const {
        return m_curtain;
    }

    void setCurtainItem(QnCurtainItem *curtain);

    void curtain(QnResourceWidget *frontWidget = NULL);

    void uncurtain();

signals:
    void curtained();
    void uncurtained();

private slots:
    void at_curtain_destroyed();
    void at_animation_finished();

private:
    void restoreFrameColor();

    void setCurtained(bool curtained);

private:
    QnCurtainItem *m_curtain;
    QColor m_curtainColor;
    QColor m_frameColor;
    bool m_curtained;
    QnAnimatorGroup *m_animatorGroup;
    QnVariantAnimator *m_curtainColorAnimator;
    QnVariantAnimator *m_frameColorAnimator;
};


#endif // QN_CURTAIN_ANIMATOR_H
