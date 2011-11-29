#ifndef QN_INSTRUMENT_PAINT_SYNCER_H
#define QN_INSTRUMENT_PAINT_SYNCER_H

#include <QObject>
#include <ui/animation/animation_timer.h>

class InstrumentPaintSyncer: public QObject, public AbstractAnimationTimer, public AnimationTimerListener {
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
    AnimationTimer *m_animationTimer;
    int m_ticksSinceLastPaint;
    QWeakPointer<QObject> m_currentWatched;
    QWidget *m_currentWidget;
};


#endif // QN_INSTRUMENT_PAINT_SYNCER_H
