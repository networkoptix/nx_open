#ifndef QN_NOPTIX_STYLE_ANIMATOR_H
#define QN_NOPTIX_STYLE_ANIMATOR_H

#include <QtCore/QObject>
#include <QtWidgets/QWidget>
#include <QHash>
#include <QtCore/QSet>
#include <ui/animation/animation_timer_listener.h>
#include <ui/animation/animation_timer.h>


class QnNoptixStyleAnimator: public QObject, protected AnimationTimerListener {
    Q_OBJECT;
public:
    QnNoptixStyleAnimator(QObject *parent = NULL):
        QObject(parent)
    {
        QAnimationTimer *animationTimer = new QAnimationTimer(this);
        setTimer(animationTimer);
        startListening();
    }

    void start(const QWidget *widget, qreal speed, qreal value) {
        if (!widget)
            return;

        if(!m_connectedWidgets.contains(widget)) {
            m_connectedWidgets.insert(widget);
            connect(widget, SIGNAL(destroyed(QObject *)), this, SLOT(at_widget_destroyed(QObject *)));
        }

        m_animationByWidget[widget] = Animation(value, speed, true);
    }

    void stop(const QWidget *widget) {
        QHash<const QWidget *, Animation>::iterator pos = m_animationByWidget.find(widget);
        if(pos == m_animationByWidget.end())
            return;

        pos->running = false;
    }

    qreal value(const QWidget *widget, qreal defaultValue = 0.0) const {
        QHash<const QWidget *, Animation>::const_iterator pos = m_animationByWidget.find(widget);
        if(pos == m_animationByWidget.end())
            return defaultValue;
        return pos->value;
    }

    void setValue(const QWidget *widget, qreal value) {
        m_animationByWidget[widget].value = value;
    }

    bool isRunning(const QWidget *widget) {
        QHash<const QWidget *, Animation>::const_iterator pos = m_animationByWidget.find(widget);
        if(pos == m_animationByWidget.end())
            return false;
        return pos->running;
    }

protected:
    virtual void tick(int deltaTime) override {
        for(QHash<const QWidget *, Animation>::iterator pos = m_animationByWidget.begin(), end = m_animationByWidget.end(); pos != end; pos++) {
            if(!pos->running)
                continue;

            const_cast<QWidget *>(pos.key())->update();
            pos->value += pos->speed * deltaTime / 1000.0;
        }
    }

protected slots:
    void at_widget_destroyed(QObject *widget) {
        m_animationByWidget.remove(static_cast<const QWidget *>(widget));
        m_connectedWidgets.remove(static_cast<const QWidget *>(widget));
    }

private:
    struct Animation {
        Animation(): value(0.0), speed(0.0), running(false) {}
        Animation(qreal value, qreal speed, bool running): value(value), speed(speed), running(running) {}

        qreal value;
        qreal speed;
        bool running;
    };

    QSet<const QWidget *> m_connectedWidgets;
    QHash<const QWidget *, Animation> m_animationByWidget;
};


#endif // QN_NOPTIX_STYLE_ANIMATOR_H
