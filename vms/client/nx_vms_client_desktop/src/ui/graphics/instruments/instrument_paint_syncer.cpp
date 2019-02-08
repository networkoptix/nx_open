#include "instrument_paint_syncer.h"

#include <QtCore/QEvent>
#include <QtCore/QDateTime>
#include <QtWidgets/QWidget>

InstrumentPaintSyncer::InstrumentPaintSyncer(QObject *parent):
    QObject(parent),
    m_animationTimer(new QAnimationTimer(this)),
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

    updateCurrentTime(QDateTime::currentMSecsSinceEpoch());
    return false;
}

void InstrumentPaintSyncer::tick(int /*deltaTime*/) {
    if(currentWatched() && m_currentWidget)
        m_currentWidget->update(); 
}

void InstrumentPaintSyncer::activatedNotify() {
    m_animationTimer->activate();
}

void InstrumentPaintSyncer::deactivatedNotify() {
    m_animationTimer->deactivate();
}
