#include "instrument_paint_syncer.h"

#include <QtCore/QEvent>
#include <QtCore/QDateTime>
#include <QtGui/QWidget>

InstrumentPaintSyncer::InstrumentPaintSyncer(QObject *parent):
    QObject(parent),
    m_animationTimer(new QAnimationTimer(this)),
    m_ticksSinceLastPaint(0),
    m_currentWidget(NULL)
{
    m_animationTimer->addListener(this);
    startListening();
}

bool InstrumentPaintSyncer::eventFilter(QObject *watched, QEvent *event) {
    if(event->type() != QEvent::UpdateRequest)
        return false;

    if(currentWatched() != watched) {
        m_currentWatched = watched;
        m_currentWidget = qobject_cast<QWidget *>(watched);
    }

    m_ticksSinceLastPaint = 0;
    updateCurrentTime(QDateTime::currentMSecsSinceEpoch());
    return false;
}

void InstrumentPaintSyncer::tick(int /*deltaTime*/) {
    m_ticksSinceLastPaint++;
    if(m_ticksSinceLastPaint <= 1)
        return;

    if(currentWatched() != NULL && m_currentWidget != NULL) {
        /* We better wait for the paint event than firing the animations here. 
         * It may cause unnecessary updates, but I guess we can live with that. */
        m_currentWidget->update(); 
    }
}

void InstrumentPaintSyncer::activatedNotify() {
    m_animationTimer->activate();
}

void InstrumentPaintSyncer::deactivatedNotify() {
    m_animationTimer->deactivate();
}
