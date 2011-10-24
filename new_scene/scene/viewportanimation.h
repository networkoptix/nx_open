#ifndef QN_VIEWPORT_ANIMATION_H
#define QN_VIEWPORT_ANIMATION_H

#include <QAbstractAnimation>
#include <cmath>
#include <QTransform>
#include <instruments/instrumentutility.h>

class ViewportAnimation: public QAbstractAnimation {
public:
    ViewportAnimation(QObject *parent = NULL): 
        QAbstractAnimation(parent),
        m_view(NULL),
        m_duration(250),
        m_scaleDelta(0.0),
        m_lastTime(0)
    {}

    void setView(QGraphicsView *view) {
        m_view = view;
    }

    void setDuration(int duration) {
        m_duration = duration;

        recalculateSpeed();
    }

    virtual int	duration() const override {
        return m_duration;
    }

    void setPositionDelta(const QPointF &positionDelta) {
        m_positionDelta = positionDelta;

        recalculateSpeed();
    }

    void setScaleDelta(qreal factor) {
        m_scaleDelta = factor - 1.0;

        recalculateSpeed();
    }

protected:
    virtual void updateCurrentTime(int currentTime) override {
        if(m_view == NULL)
            return;

        qreal t = currentTime / 1000.0;

        if(state() == Running) {
            QPointF currentPosition = m_view->mapToScene(m_view->rect().center());

            m_startingPosition += currentPosition - m_lastPosition;
            InstrumentUtility::moveViewport(m_view, m_startingPosition + t * m_positionSpeed - currentPosition);

            qDebug("%g %g", (currentPosition - m_lastPosition).x(), (currentPosition - m_lastPosition).y());

            qreal factor = (1.0 + t * m_scaleSpeed) / (1.0 + m_lastTime * m_scaleSpeed);
            InstrumentUtility::scaleViewport(m_view, factor);

            m_lastPosition = m_view->mapToScene(m_view->rect().center());
        }

        m_lastTime = t;
    }

    virtual void updateState(State newState, State /*oldState*/) override {
        if(newState != Running)
            return;

        if(m_view == NULL)
            return;

        m_lastTime = currentTime() / 1000.0;
        m_startingPosition = m_view->mapToScene(m_view->viewport()->rect().center());
        m_lastPosition = m_startingPosition;
    }

private:
    void recalculateSpeed() {
        qreal totalTime = duration() / 1000.0;
        m_positionSpeed = m_positionDelta / totalTime;
        m_scaleSpeed = m_scaleDelta / totalTime;
    }

private:
    QGraphicsView *m_view;
    int m_duration;
    QPointF m_positionDelta;
    qreal m_scaleDelta;
    
    QPointF m_positionSpeed;
    qreal m_scaleSpeed;

    QPointF m_startingPosition;
    QPointF m_lastPosition;

    qreal m_lastTime;
};


#endif // QN_VIEWPORT_ANIMATION_H
