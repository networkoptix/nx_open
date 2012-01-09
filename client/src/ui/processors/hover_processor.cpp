#include "hover_processor.h"
#include <QTimerEvent>
#include <utils/common/warnings.h>

HoverProcessor::HoverProcessor(QGraphicsItem *parent):
    QGraphicsObject(parent),
    m_hoverEnterDelay(0),
    m_hoverLeaveDelay(0),
    m_hoverEnterTimerId(0),
    m_hoverLeaveTimerId(0),
    m_item(NULL)
{}

HoverProcessor::~HoverProcessor() {
    return;
}

void HoverProcessor::setHoverEnterDelay(int hoverEnterDelayMSec) {
    if(hoverEnterDelayMSec < 0) {
        qnWarning("Invalid negative hover enter delay '%1'.", hoverEnterDelayMSec);
        return;
    }

    m_hoverEnterDelay = hoverEnterDelayMSec;
}

void HoverProcessor::setHoverLeaveDelay(int hoverLeaveDelayMSec) {
    if(hoverLeaveDelayMSec < 0) {
        qnWarning("Invalid negative hover leave delay '%1'.", hoverLeaveDelayMSec);
        return;
    }

    m_hoverLeaveDelay = hoverLeaveDelayMSec;
}


void HoverProcessor::setTargetItem(QGraphicsItem *item) {
    if(!m_item.isNull())
        m_item.data()->removeSceneEventFilter(this);
    
    m_item = item;

    if(!m_item.isNull()) {
        m_item.data()->installSceneEventFilter(this);
        m_item.data()->setAcceptHoverEvents(true);
    }
}

void HoverProcessor::hoverEnterEvent(QGraphicsSceneHoverEvent *event) {
    killHoverLeaveTimer();

    if(m_hoverEnterDelay == 0) {
        processEnter();
    } else {
        m_hoverEnterTimerId = startTimer(m_hoverEnterDelay);
    }
}

void HoverProcessor::hoverLeaveEvent(QGraphicsSceneHoverEvent *event) {
    killHoverEnterTimer();

    if(m_hoverLeaveDelay == 0) {
        processLeave();
    } else {
        m_hoverLeaveTimerId = startTimer(m_hoverLeaveDelay);
    }
}

void HoverProcessor::timerEvent(QTimerEvent *event) {
    if(event->timerId() == m_hoverEnterTimerId) {
        processEnter();
    } else if(event->timerId() == m_hoverLeaveTimerId) {
        processLeave();
    }
}

void HoverProcessor::processEnter() {
    killHoverEnterTimer();

    emit hoverEntered(m_item.data());
}

void HoverProcessor::processLeave() {
    killHoverLeaveTimer();

    emit hoverLeft(m_item.data());
}

void HoverProcessor::killHoverEnterTimer() {
    if(m_hoverEnterTimerId == 0)
        return;
    
    killTimer(m_hoverEnterTimerId);
    m_hoverEnterTimerId = 0;
}

void HoverProcessor::killHoverLeaveTimer() {
    if(m_hoverLeaveTimerId == 0)
        return;

    killTimer(m_hoverLeaveTimerId);
    m_hoverLeaveTimerId = 0;
}
