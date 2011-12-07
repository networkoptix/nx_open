#include "viewport_animator.h"
#include <cmath> /* For std::log, std::exp. */
#include <limits>
#include <QParallelAnimationGroup>
#include <ui/common/scene_utility.h>
#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include "setter_animation.h"

namespace {
    class ViewportPositionSetter {
    public:
        void operator()(QObject *view, const QVariant &value) const {
            QnSceneUtility::moveViewportSceneTo(checked_cast<QGraphicsView *>(view), value.toPointF());
        }
    };

    class ViewportScaleSetter {
    public:
        ViewportScaleSetter(Qt::AspectRatioMode mode): m_mode(mode) {}

        void operator()(QObject *view, const QVariant &value) const {
            QnSceneUtility::scaleViewportTo(checked_cast<QGraphicsView *>(view), value.toSizeF(), m_mode);
        }

    private:
        Qt::AspectRatioMode m_mode;
    };

} // anonymous namespace


QnViewportAnimator::QnViewportAnimator(QObject *parent):
    QObject(parent),
    m_view(NULL),
    m_movementSpeed(1.0),
    m_logScalingSpeed(std::log(2.0)),
    m_animationGroup(NULL),
    m_scaleAnimation(NULL),
    m_positionAnimation(NULL)
{
    m_animationGroup = new QParallelAnimationGroup(this);
    connect(m_animationGroup, SIGNAL(stateChanged(QAbstractAnimation::State, QAbstractAnimation::State)), this, SLOT(at_animationGroup_stateChanged(QAbstractAnimation::State, QAbstractAnimation::State)));
}

void QnViewportAnimator::setView(QGraphicsView *view) {
    if(m_view != NULL) {
        m_animationGroup->stop();

        delete m_scaleAnimation;
        m_scaleAnimation = NULL;

        delete m_positionAnimation;
        m_positionAnimation = NULL;
    }

    m_view = view;

    if(m_view != NULL) {
        if(m_view->thread() != thread()) {
            qnWarning("Cannot create an animator for a graphics view in another thread.");
            return;
        }

        connect(m_view, SIGNAL(destroyed()), this, SLOT(at_view_destroyed()));

        m_scaleAnimation = new QnSetterAnimation(this);
        m_scaleAnimation->setSetter(newSetter(ViewportScaleSetter(Qt::KeepAspectRatioByExpanding)));
        m_scaleAnimation->setTargetObject(view);

        m_positionAnimation = new QnSetterAnimation(this);
        m_positionAnimation->setSetter(newSetter(ViewportPositionSetter()));
        m_positionAnimation->setTargetObject(view);

        m_animationGroup->addAnimation(m_scaleAnimation);
        m_animationGroup->addAnimation(m_positionAnimation);
    }
}

void QnViewportAnimator::at_view_destroyed() {
    setView(NULL);
}

void QnViewportAnimator::at_animationGroup_stateChanged(QAbstractAnimation::State newState, QAbstractAnimation::State oldState) {
    if(newState == QAbstractAnimation::Stopped) {
        emit animationFinished();
    } else if(oldState == QAbstractAnimation::Stopped) {
        emit animationStarted();
    }
}

void QnViewportAnimator::moveTo(const QRectF &rect, int timeLimitMsecs) {
    if(m_view == NULL)
        return;

    if(timeLimitMsecs == -1)
        timeLimitMsecs = std::numeric_limits<int>::max();

    /* Adjust for margins. */
    QSizeF viewportSize = m_view->viewport()->size();
    MarginsF extension = cwiseDiv(m_viewportMargins, viewportSize);
    extension = cwiseMul(cwiseDiv(extension, MarginsF(1.0, 1.0, 1.0, 1.0) - extension), rect.size());
    QRectF actualRect = dilated(rect, extension);


    QRectF viewportRect = mapRectToScene(m_view, m_view->viewport()->rect());
    if(qFuzzyCompare(viewportRect, actualRect)) {
        m_animationGroup->stop();
        return;
    }

    qreal timeLimit = timeLimitMsecs / 1000.0;
    qreal movementTime = length(actualRect.center() - viewportRect.center()) / (m_movementSpeed * length(viewportRect.size() + actualRect.size()) / 2);
    qreal scalingTime = std::abs(std::log(scaleFactor(viewportRect.size(), actualRect.size(), Qt::KeepAspectRatioByExpanding)) / m_logScalingSpeed);

    int durationMsecs = 1000 * qMin(timeLimit, qMax(movementTime, scalingTime));
    if(durationMsecs == 0) {
        m_animationGroup->stop();
        return;
    }

    m_targetRect = actualRect;

    bool alreadyAnimating = isAnimating();
    bool signalsBlocked = blockSignals(true); /* Don't emit animationFinished() now. */

    m_scaleAnimation->setStartValue(viewportRect.size());
    m_scaleAnimation->setEndValue(actualRect.size());
    m_scaleAnimation->setDuration(durationMsecs);

    m_positionAnimation->setStartValue(viewportRect.center());
    m_positionAnimation->setEndValue(actualRect.center());
    m_positionAnimation->setDuration(durationMsecs);

    m_animationGroup->setCurrentTime(0);
    m_animationGroup->setDirection(QAbstractAnimation::Forward);

    m_animationGroup->start();
    blockSignals(signalsBlocked);
    if(!alreadyAnimating)
        emit animationStarted();
}

bool QnViewportAnimator::isAnimating() const {
    return m_animationGroup->state() == QAbstractAnimation::Running;
}

#if 0
void QnViewportAnimator::reverse() {
    if(!isAnimating())
        return;

    m_animationGroup->setDirection(m_animationGroup->direction() == QAbstractAnimation::Forward ? QAbstractAnimation::Backward : QAbstractAnimation::Forward);
}
#endif

qreal QnViewportAnimator::movementSpeed() const {
    return m_movementSpeed;
}

qreal QnViewportAnimator::scalingSpeed() const {
    return std::exp(m_logScalingSpeed);
}

void QnViewportAnimator::setMovementSpeed(qreal multiplier) {
    m_movementSpeed = multiplier;
}

void QnViewportAnimator::setScalingSpeed(qreal multiplier) {
    m_logScalingSpeed = std::log(multiplier);
}

const QRectF &QnViewportAnimator::targetRect() const {
    return m_targetRect;
}