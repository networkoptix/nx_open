#include "display_state.h"
#include <cassert>
#include <utils/common/warnings.h>
#include <ui/model/layout_model.h>
#include <ui/model/layout_grid_mapper.h>
#include <ui/model/resource_item_model.h>

QnDisplayState::QnDisplayState(QnLayoutModel *model, QObject *parent):
    QObject(parent),
    m_model(model),
    m_gridMapper(new QnLayoutGridMapper(this)),
    m_selectedEntity(NULL),
    m_zoomedEntity(NULL)
{
    assert(model != NULL);

    connect(model, SIGNAL(itemAboutToBeRemoved(QnLayoutItemModel *)), this, SLOT(at_entity_aboutToBeRemoved(QnLayoutItemModel *)));
}    

QnDisplayState::~QnDisplayState() {
    return;
}

void QnDisplayState::setMode(Mode mode) {
    if(m_mode == mode)
        return;

    m_mode = mode;

    emit modeChanged();
}

void QnDisplayState::setSelectedEntity(QnLayoutItemModel *entity) {
    if(m_selectedEntity == entity)
        return;

    if(entity != NULL && entity->layout() != m_model) {
        qnWarning("Cannot select an entity from another model.");
        return;
    }

    QnLayoutItemModel *oldSelectedEntity = m_selectedEntity;
    m_selectedEntity = entity;
    
    emit selectedEntityChanged(oldSelectedEntity, m_selectedEntity);
}

void QnDisplayState::setZoomedEntity(QnLayoutItemModel *entity) {
    if(m_zoomedEntity == entity)
        return;

    if(entity != NULL && entity->layout() != m_model) {
        qnWarning("Cannot zoom to an entity from another model.");
        return;
    }

    QnLayoutItemModel *oldZoomedEntity = m_zoomedEntity;
    m_zoomedEntity = entity;

    emit zoomedEntityChanged(oldZoomedEntity, m_zoomedEntity);
}

void QnDisplayState::at_entity_aboutToBeRemoved(QnLayoutItemModel *entity) {
    if(entity == m_selectedEntity)
        setSelectedEntity(NULL);

    if(entity == m_zoomedEntity)
        setZoomedEntity(NULL);
}