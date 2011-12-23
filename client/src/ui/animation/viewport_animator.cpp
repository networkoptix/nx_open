#include "viewport_animator.h"
#include <cmath> /* For std::log, std::exp. */
#include <limits>
#include <QGraphicsView>
#include <QMargins>
#include <ui/common/scene_utility.h>
#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include <ui/common/linear_combination.h>
#include <ui/common/magnitude.h>
#include "setter_animation.h"
#include "accessor.h"

class QnViewportRectAccessor: public QnAbstractAccessor {
public:
    virtual QVariant get(const QObject *object) const override {
        const QGraphicsView *view = checked_cast<const QGraphicsView *>(object);

        return SceneUtility::mapRectToScene(view, SceneUtility::eroded(view->viewport()->rect(), m_margins));
    }

    virtual void set(QObject *object, const QVariant &value) const override {
        QGraphicsView *view = checked_cast<QGraphicsView *>(object);
        QRectF sceneRect = value.toRectF();

        /* Extend to match aspect ratio. */
        sceneRect = aspectRatioAdjusted(view, sceneRect);

        /* Adjust for margins. */
        MarginsF extension = SceneUtility::cwiseDiv(m_margins, QSizeF(view->viewport()->size()));
        extension = SceneUtility::cwiseMul(SceneUtility::cwiseDiv(extension, MarginsF(1.0, 1.0, 1.0, 1.0) - extension), sceneRect.size());
        sceneRect = SceneUtility::dilated(sceneRect, extension);

        /* Set! */
        SceneUtility::moveViewportSceneTo(view, sceneRect.center());
        SceneUtility::scaleViewportTo(view, sceneRect.size(), Qt::KeepAspectRatioByExpanding);
    }

    const QMargins &margins() const {
        return m_margins;
    }

    void setMargins(const QMargins &margins) {
        m_margins = margins;
    }

    QRectF aspectRatioAdjusted(QGraphicsView *view, const QRectF &sceneRect) const {
        return SceneUtility::expanded(
            SceneUtility::aspectRatio(SceneUtility::eroded(view->viewport()->size(), m_margins)), 
            sceneRect, 
            Qt::KeepAspectRatioByExpanding, 
            Qt::AlignCenter
        );
    }

private:
    QMargins m_margins;
};

QnViewportAnimator::QnViewportAnimator(QObject *parent):
    QnVariantAnimator(parent),
    m_accessor(new QnViewportRectAccessor()),
    m_relativeSpeed(0.0)
{
    setAccessor(m_accessor);
}

void QnViewportAnimator::setView(QGraphicsView *view) {
    stop();

    setTargetObject(view);
}

QGraphicsView *QnViewportAnimator::view() const {
    return checked_cast<QGraphicsView *>(targetObject());
}

void QnViewportAnimator::moveTo(const QRectF &rect) {
    pause();
    
    setTargetValue(rect);

    start();
}

const QMargins &QnViewportAnimator::viewportMargins() const {
    return m_accessor->margins();
}

void QnViewportAnimator::setViewportMargins(const QMargins &margins) {
    m_accessor->setMargins(margins);
}

void QnViewportAnimator::setRelativeSpeed(qreal relativeSpeed) {
    m_relativeSpeed = relativeSpeed;
}

int QnViewportAnimator::estimatedDuration() const {
    QGraphicsView *view = this->view();
    QRectF startRect = m_accessor->aspectRatioAdjusted(view, startValue().toRectF());
    QRectF targetRect = m_accessor->aspectRatioAdjusted(view, targetValue().toRectF());

    qreal speed;
    if(qFuzzyIsNull(m_relativeSpeed)) {
        speed = this->speed(); 
    } else {
        speed = m_relativeSpeed * (SceneUtility::length(startRect.size()) + SceneUtility::length(targetRect.size())) / 2;
    }

    return magnitudeCalculator()->calculate(linearCombinator()->combine(1.0, startRect, -1.0, targetRect)) / speed * 1000;
}

QRectF QnViewportAnimator::targetRect() const {
    return targetValue().toRectF();
}