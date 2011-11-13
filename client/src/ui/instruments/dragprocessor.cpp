#include "dragprocessor.h"
#include <utils/common/warnings.h>

DragProcessListener::DragProcessListener():
    m_processor(NULL) 
{}

DragProcessListener::~DragProcessListener() {
    if(m_processor != NULL)
        m_processor->setListener(NULL);
}

DragProcessor::DragProcessor(QObject *parent):
    QObject(parent),
    m_listener(NULL)
{}

DragProcessor::~DragProcessor() {
    setListener(NULL);
}

void DragProcessor::mousePressEvent(QWidget *viewport, QMouseEvent *event) {

}

void DragProcessor::mouseMoveEvent(QWidget *viewport, QMouseEvent *event) {

}

void DragProcessor::mouseReleaseEvent(QWidget *viewport, QMouseEvent *event) {

}


void DragProcessor::setListener(DragProcessListener *listener) {
    if(listener->m_processor != NULL) {
        qnWarning("Given listener is already assigned to a processor.");
        return;
    }

    if(m_listener != NULL) {
        stopDragProcess();
        m_listener->m_processor = NULL;
    }

    m_listener = listener;

    if(m_listener != NULL)
        m_listener->m_processor = this;
}
