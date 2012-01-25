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
#include "accessor.h"

class ViewportRectAccessor: public AbstractAccessor {
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

ViewportAnimator::ViewportAnimator(QObject *parent):
    base_type(parent),
    m_accessor(new ViewportRectAccessor()),
    m_relativeSpeed(0.0)
{
    setAccessor(m_accessor);
}

ViewportAnimator::~ViewportAnimator() {
    stop();
}

void ViewportAnimator::setView(QGraphicsView *view) {
    stop();

    setTargetObject(view);
}

QGraphicsView *ViewportAnimator::view() const {
    return checked_cast<QGraphicsView *>(targetObject());
}

void ViewportAnimator::moveTo(const QRectF &rect) {
    pause();
    
    setTargetValue(rect);

    start();
}

const QMargins &ViewportAnimator::viewportMargins() const {
    return m_accessor->margins();
}

void ViewportAnimator::setViewportMargins(const QMargins &margins) {
    if(margins == m_accessor->margins())
        return;

    QRectF targetValue = this->targetValue().toRectF();

    m_accessor->setMargins(margins);

    if(isRunning()) {
        /* Margins were changed, restart needed to avoid jitter. */
        pause();
        setTargetValue(targetValue);
        start();
    }
}

int ViewportAnimator::estimatedDuration(const QVariant &from, const QVariant &to) const {
    QGraphicsView *view = this->view();
    QRectF startRect = m_accessor->aspectRatioAdjusted(view, from.toRectF());
    QRectF targetRect = m_accessor->aspectRatioAdjusted(view, to.toRectF());

    return base_type::estimatedDuration(startRect, targetRect);
}

QRectF ViewportAnimator::targetRect() const {
    return targetValue().toRectF();
}

