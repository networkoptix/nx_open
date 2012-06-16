#ifndef QN_INSTRUMENT_PAINT_SYNCER_H
#define QN_INSTRUMENT_PAINT_SYNCER_H

#include <QObject>
#include <QWeakPointer>
#include <ui/animation/animation_timer.h>

class InstrumentPaintSyncer: public QObject, public AnimationTimer, public AnimationTimerListener {
public:
    InstrumentPaintSyncer(QObject *parent = NULL);

    virtual bool eventFilter(QObject *filtered, QEvent *event);

protected:
    virtual void tick(int deltaTime) override;

    virtual void activatedNotify() override;
    virtual void deactivatedNotify() override;

    QObject *currentWatched() const {
        return m_currentWatched.data();
    }

private:
    QAnimationTimer *m_animationTimer;
    QWeakPointer<QObject> m_currentWatched;
    QWidget *m_currentWidget;
};


#endif // QN_INSTRUMENT_PAINT_SYNCER_H
