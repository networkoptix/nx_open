// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "instrumented.h"
#include "instrument_manager.h"

void InstrumentedBase::updateScene(QGraphicsScene *scene, QGraphicsItem *item) {
    if(m_scene == scene)
        return;

    if(InstrumentManager *manager = InstrumentManager::instance(m_scene))
        manager->unregisterItem(item);

    m_scene = scene;

    if(InstrumentManager *manager = InstrumentManager::instance(m_scene))
        manager->registerItem(item, true);
}
