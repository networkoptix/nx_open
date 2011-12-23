#ifndef QN_CURTAIN_ANIMATOR_H
#define QN_CURTAIN_ANIMATOR_H

#include <QObject>
#include <QWeakPointer>
#include "animator_group.h"

class QnCurtainItem;
class QnResourceWidget;
class QnVariantAnimator;

class QnCurtainAnimator: public QnAnimatorGroup {
    Q_OBJECT;

public:
    QnCurtainAnimator(QObject *parent = NULL);

    QnCurtainItem *curtainItem() const;

    void setCurtainItem(QnCurtainItem *curtain);

    void curtain(QnResourceWidget *frontWidget = NULL);

    void uncurtain();

    void setSpeed(qreal speed);

signals:
    void curtained();
    void uncurtained();

private slots:
    void at_animation_finished();

private:
    void restoreFrameColor();

    void setCurtained(bool curtained);

private:
    QColor m_curtainColor;
    QColor m_frameColor;
    bool m_curtained;
    QnVariantAnimator *m_curtainColorAnimator;
    QnVariantAnimator *m_frameColorAnimator;
};


#endif // QN_CURTAIN_ANIMATOR_H
