// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "drag_process_handler.h"
#include "drag_processor.h"

DragProcessHandler::DragProcessHandler():
    m_processor(nullptr)
{}

DragProcessHandler::~DragProcessHandler() {
    if(m_processor != nullptr)
        m_processor->setHandler(nullptr);
}
