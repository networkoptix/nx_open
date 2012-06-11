#include "instrumented.h"
#include "instrument_manager.h"

detail::InstrumentedBase::InstrumentedBase(): m_scene(NULL) {}

void detail::InstrumentedBase::updateScene(QGraphicsScene *scene, QGraphicsItem *item) {
    if(m_scene == scene)
        return;

    if(m_scene != NULL)
        foreach(InstrumentManager *manager, InstrumentManager::managersOf(m_scene))
            manager->unregisterItem(item);

    m_scene = scene;

    if(m_scene != NULL)
        foreach(InstrumentManager *manager, InstrumentManager::managersOf(m_scene))
            manager->registerItem(item, true);
}
        
