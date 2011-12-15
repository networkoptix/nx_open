#include "redirectinginstrument.h"
#include <utils\common\warnings.h>

void RedirectingInstrument::init(Instrument *target) {
    if(target == NULL)
        qnNullWarning(target);

    m_target = target;
}

bool RedirectingInstrument::event(QGraphicsScene *scene, QEvent *event) {
    if(m_target != NULL)
        return m_target->event(scene, event);
}

bool RedirectingInstrument::event(QGraphicsView *view, QEvent *event) {
    if(m_target != NULL)
        return m_target->event(view, event);
}

bool RedirectingInstrument::event(QWidget *viewport, QEvent *event) {
    if(m_target != NULL)
        return m_target->event(viewport, event);
}

bool RedirectingInstrument::sceneEvent(QGraphicsItem *item, QEvent *event) {
    if(m_target != NULL)
        return m_target->sceneEvent(item, event);
}

