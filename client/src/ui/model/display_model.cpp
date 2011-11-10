#include "display_model.h"
#include <utils/common/warnings.h>
#include "display_entity.h"

QnDisplayModel::QnDisplayModel(QObject *parent):
    QObject(parent)
{}


QnDisplayModel::~QnDisplayModel() {
    clear();
}

void QnDisplayModel::clear() {
    while(!m_entities.empty()) {
        QnDisplayEntity *entity = *m_entities.begin();

        removeEntity(entity);
        delete entity;
    }
}

void QnDisplayModel::addEntity(QnDisplayEntity *entity) {
    if(entity == NULL) {
        qnNullWarning(entity);
        return;
    }

    if(entity->model() != NULL)
        entity->model()->removeEntity(entity);

    if(entity->isPinned() && m_entityMap.isOccupied(entity->geometry())) 
        entity->setFlag(QnDisplayEntity::Pinned, false);

    entity->m_model = this;
    m_entities.insert(entity);
    if(entity->isPinned())
        m_entityMap.fill(entity->geometry(), entity);
    m_rectSet.insert(entity->geometry());

    emit entityAdded(entity);
}

void QnDisplayModel::removeEntity(QnDisplayEntity *entity) {
    if(entity == NULL) {
        qnNullWarning(entity);
        return;
    }

    if(entity->model() != this) {
        qnWarning("Cannot remove an entity that belongs to a different model.");
        return;
    }

    emit entityAboutToBeRemoved(entity);

    if(entity->isPinned())
        m_entityMap.clear(entity->geometry());
    m_rectSet.remove(entity->geometry());
    entity->m_model = NULL;
    m_entities.remove(entity);

    delete entity;
}

bool QnDisplayModel::moveEntity(QnDisplayEntity *entity, const QRect &geometry) {
    if(entity->model() != this) {
        qnWarning("Cannot move an entity that does not belong to this model");
        return false;
    }

    if (entity->isPinned() && !m_entityMap.isOccupiedBy(geometry, entity, true))
        return false;

    if(entity->isPinned()) {
        m_entityMap.clear(entity->geometry());
        m_entityMap.fill(geometry, entity);
    }

    moveEntityInternal(entity, geometry);

    return true;
}

void QnDisplayModel::moveEntityInternal(QnDisplayEntity *entity, const QRect &geometry) {
    m_rectSet.remove(entity->geometry());
    m_rectSet.insert(geometry);
    entity->setGeometryInternal(geometry);
}

bool QnDisplayModel::moveEntities(const QList<QnDisplayEntity *> &entities, const QList<QRect> &geometries) {
    if (entities.size() != geometries.size()) {
        qnWarning("Sizes of the given containers do not match.");
        return false;
    }

    if (entities.empty())
        return true;

    /* Check whether it's our entities. */
    foreach (QnDisplayEntity *entity, entities) {
        if (entity->model() != this) {
            qnWarning("One of the given entities does not belong to this model.");
            return false;
        }
    }

    /* Check whether new positions do not intersect each other. */
    QSet<QPoint> pointSet;
    for(int i = 0; i < entities.size(); i++) {
        if(!entities[i]->isPinned())
            continue;

        const QRect &geometry = geometries[i];
        for (int r = geometry.top(); r <= geometry.bottom(); r++) {
            for (int c = geometry.left(); c <= geometry.right(); c++) {
                QPoint point(c, r);

                if (pointSet.contains(point))
                    return false;

                pointSet.insert(point);
            }
        }
    }

    /* Check validity of new positions relative to existing items. */
    QSet<QnDisplayEntity *> replacedEntities;
    for(int i = 0; i < entities.size(); i++) {
        if(!entities[i]->isPinned())
            continue;

        m_entityMap.values(geometries[i], &replacedEntities);
    }
    foreach (QnDisplayEntity *entity, entities)
        replacedEntities.remove(entity);
    if (!replacedEntities.empty())
        return false;

    /* Move. */
    foreach (QnDisplayEntity *entity, entities)
        if(entity->isPinned())
            m_entityMap.clear(entity->geometry());
    for (int i = 0; i < entities.size(); i++) {
        QnDisplayEntity *entity = entities[i];

        if(entity->isPinned())
            m_entityMap.fill(geometries[i], entity);
        moveEntityInternal(entity, geometries[i]);
    }

    return true;
}

bool QnDisplayModel::pinEntity(QnDisplayEntity *entity, const QRect &geometry) {
    if(entity->model() != this) {
        qnWarning("Cannot pin an entity that does not belong to this model");
        return false;
    }

    if(entity->isPinned())
        return moveEntity(entity, geometry);

    if(m_entityMap.isOccupied(geometry))
        return false;

    m_entityMap.fill(geometry, entity);
    moveEntityInternal(entity, geometry);
    entity->setFlagInternal(QnDisplayEntity::Pinned, true);
    return true;
}

bool QnDisplayModel::unpinEntity(QnDisplayEntity *entity) {
    if(entity->model() != this) {
        qnWarning("Cannot unpin an entity that does not belong to this model");
        return false;
    }

    if(!entity->isPinned())
        return true;

    m_entityMap.clear(entity->geometry());
    entity->setFlagInternal(QnDisplayEntity::Pinned, false);
    return true;
}

QnDisplayEntity *QnDisplayModel::entity(const QPoint &position) const {
    return m_entityMap.value(position, NULL);
}

QSet<QnDisplayEntity *> QnDisplayModel::entities(const QRect &region) const {
    return m_entityMap.values(region);
}

QSet<QnDisplayEntity *> QnDisplayModel::entities(const QList<QRect> &regions) const {
    return m_entityMap.values(regions);
}

namespace {
    bool isFree(const QnMatrixMap<QnDisplayEntity *> &map, const QPoint &pos, const QSize &size, int dr, int dc, QRect *result) {
        QRect rect = QRect(pos + QPoint(dc, dr), size);

        if(!map.isOccupied(rect)) {
            *result = rect;
            return true;
        } else {
            return false;
        }
    }
}

QRect QnDisplayModel::closestFreeSlot(const QPoint &pos, const QSize &size) const {
    QRect result;

    for(int l = 0; ; l++) {
        int dr, dc;

        /* Iterate in the following order:
         *  213
         *  2.3
         *  444
         */

        /* 1. */
        dr = -l;
        for(dc = -l + 1; dc <= l - 1; dc++)
            if(isFree(m_entityMap, pos, size, dr, dc, &result))
                return result;

        /* 2. */
        dc = -l;
        for(dr = -l; dr <= l - 1; dr++)
            if(isFree(m_entityMap, pos, size, dr, dc, &result))
                return result;

        /* 3. */
        dc = l;
        for(dr = -l; dr <= l - 1; dr++)
            if(isFree(m_entityMap, pos, size, dr, dc, &result))
                return result;

        /* 4. */
        dr = l;
        for(dc = -l; dc <= l; dc++)
            if(isFree(m_entityMap, pos, size, dr, dc, &result))
                return result;
    }
}