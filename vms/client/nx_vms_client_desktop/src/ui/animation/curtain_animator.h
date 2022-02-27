// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_CURTAIN_ANIMATOR_H
#define QN_CURTAIN_ANIMATOR_H

#include <QtGui/QColor>
#include <QtCore/QObject>


#include "animator_group.h"

class QnCurtainItem;
class QnResourceWidget;
class VariantAnimator;

class QnCurtainAnimator: public AnimatorGroup {
    Q_OBJECT;

public:
    QnCurtainAnimator(QObject *parent = nullptr);

    virtual ~QnCurtainAnimator();

    QnCurtainItem *curtainItem() const;

    void setCurtainItem(QnCurtainItem *curtain);

    void curtain(QnResourceWidget *frontWidget = nullptr);

    void uncurtain();

    void setSpeed(qreal speed);

    bool isCurtained() const;

signals:
    void curtained();
    void uncurtained();

private slots:
    void at_animation_finished();

private:
    void restoreFrameColor();

    void setCurtained(bool curtained);

private:
    qreal m_frameOpacity;
    bool m_curtained;
    VariantAnimator *m_curtainOpacityAnimator;
    VariantAnimator *m_frameOpacityAnimator;
};


#endif // QN_CURTAIN_ANIMATOR_H
