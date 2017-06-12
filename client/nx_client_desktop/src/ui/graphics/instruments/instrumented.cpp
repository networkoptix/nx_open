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
        
