#include "drag_process_handler.h"
#include "drag_processor.h"

DragProcessHandler::DragProcessHandler():
    m_processor(NULL) 
{}

DragProcessHandler::~DragProcessHandler() {
    if(m_processor != NULL)
        m_processor->setHandler(NULL);
}
