#include "display_state.h"
#include <cassert>
#include <utils/common/warnings.h>
#include <ui/model/display_model.h>
#include <ui/model/display_entity.h>
#include "display_grid_mapper.h"

QnDisplayState::QnDisplayState(QnDisplayModel *model, QObject *parent):
    QObject(parent),
    m_model(model),
    m_gridMapper(new QnDisplayGridMapper(this)),
    m_selectedEntity(NULL),
    m_zoomedEntity(NULL)
{
    assert(model != NULL);

    connect(model, SIGNAL(entityAboutToBeRemoved(QnDisplayEntity *)), this, SLOT(at_entity_aboutToBeRemoved(QnDisplayEntity *)));
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

void QnDisplayState::setSelectedEntity(QnDisplayEntity *entity) {
    if(m_selectedEntity == entity)
        return;

    if(entity != NULL && entity->model() != m_model) {
        qnWarning("Cannot select an entity from another model.");
        return;
    }

    QnDisplayEntity *oldSelectedEntity = m_selectedEntity;
    m_selectedEntity = entity;
    
    emit selectedEntityChanged(oldSelectedEntity, m_selectedEntity);
}

void QnDisplayState::setZoomedEntity(QnDisplayEntity *entity) {
    if(m_zoomedEntity == entity)
        return;

    if(entity != NULL && entity->model() != m_model) {
        qnWarning("Cannot zoom to an entity from another model.");
        return;
    }

    QnDisplayEntity *oldZoomedEntity = m_zoomedEntity;
    m_zoomedEntity = entity;

    emit zoomedEntityChanged(oldZoomedEntity, m_zoomedEntity);
}

void QnDisplayState::at_entity_aboutToBeRemoved(QnDisplayEntity *entity) {
    if(entity == m_selectedEntity)
        setSelectedEntity(NULL);

    if(entity == m_zoomedEntity)
        setZoomedEntity(NULL);
}