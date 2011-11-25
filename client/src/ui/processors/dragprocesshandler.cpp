#include "dragprocesshandler.h"
#include "dragprocessor.h"

DragProcessHandler::DragProcessHandler():
    m_processor(NULL) 
{}

DragProcessHandler::~DragProcessHandler() {
    if(m_processor != NULL)
        m_processor->setHandler(NULL);
}
