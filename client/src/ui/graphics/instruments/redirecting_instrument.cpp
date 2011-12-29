#include "redirecting_instrument.h"
#include <utils/common/warnings.h>

void RedirectingInstrument::init(Instrument *target) {
    if(target == NULL)
        qnNullWarning(target);

    m_target = target;
}

bool RedirectingInstrument::event(QGraphicsScene *scene, QEvent *event) {
    if(m_target != NULL) {
        return m_target->event(scene, event);
    } else {
        return false;
    }
}

bool RedirectingInstrument::event(QGraphicsView *view, QEvent *event) {
    if(m_target != NULL) {
        return m_target->event(view, event);
    } else {
        return false;
    }
}

bool RedirectingInstrument::event(QWidget *viewport, QEvent *event) {
    if(m_target != NULL) {
        return m_target->event(viewport, event);
    } else {
        return false;
    }
}

bool RedirectingInstrument::sceneEvent(QGraphicsItem *item, QEvent *event) {
    if(m_target != NULL) {
        return m_target->sceneEvent(item, event);
    } else {
        return false;
    }
}

