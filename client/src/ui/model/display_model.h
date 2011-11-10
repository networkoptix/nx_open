#ifndef QN_DISPLAY_MODEL_H
#define QN_DISPLAY_MODEL_H

#include <QObject>
#include <QSet>
#include <utils/common/matrix_map.h>
#include <utils/common/rect_set.h>

class QnDisplayEntity;

class QnDisplayModel: public QObject {
    Q_OBJECT;
public:
    /**
     * Constructor.
     * 
     * \param parent                    Parent object for this display model.
     */
    QnDisplayModel(QObject *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~QnDisplayModel();

    /**
     * Adds the given entity to this model. This model takes ownership of the
     * given entity. If the given entity already belongs to some other model,
     * it will first be removed from that model.
     * 
     * If the position where the entity is to be placed is occupied, the entity
     * will be placed unpinned.
     * 
     * \param entity                    Entity to add.
     */
    void addEntity(QnDisplayEntity *entity);

    /**
     * Removes the given entity from this model. Entity's ownership is passed
     * to the caller.
     * 
     * \param entity                    Entity to remove
     */
    void removeEntity(QnDisplayEntity *entity);
    
    /**
     * \param entity                    Entity to move to a new position.
     * \param geometry                  New position.
     * \returns                         Whether the entity was moved.
     */
    bool moveEntity(QnDisplayEntity *entity, const QRect &geometry);

    /**
     * \param entities                  Entities to move to new positions.
     * \param geometries                New positions.
     * \returns                         Whether the entities were moved.
     */
    bool moveEntities(const QList<QnDisplayEntity *> &entities, const QList<QRect> &geometries);

    /**
     * \param entity                    Entity to pin.
     * \param geometry                  Position to pin to.
     * \returns                         Whether the entity was pinned.
     */
    bool pinEntity(QnDisplayEntity *entity, const QRect &geometry);

    /**
     * \param entity                    Entity to unpin.
     * \returns                         Whether the entity was unpinned.
     */
    bool unpinEntity(QnDisplayEntity *entity);

    /**
     * \param position                  Position to get entity at.
     * \returns                         Entity at the given position, or NULL if the given position is empty.
     */
    QnDisplayEntity *entity(const QPoint &position) const;

    /**
     * \param region                    Region to get entities at.
     * \returns                         Set of entities at the given region.
     */
    QSet<QnDisplayEntity *> entities(const QRect &region) const;

    /**
     * \param regions                   Regions to get entities at.
     * \returns                         Set of entities at the given regions.
     */
    QSet<QnDisplayEntity *> entities(const QList<QRect> &regions) const;

    /**
     * \returns                         All entities of this model.
     */
    const QSet<QnDisplayEntity *> &entities() const {
        return m_entities;
    }

    /**
     * Clears this model by removing all its entities.
     */
    void clear();

    /**
     * \returns                         Bounding rectangle of all entities in this model.
     */
    QRect boundingRect() const {
        return m_rectSet.boundingRect();
    }

    /**
     * \param pos                       Desired position.
     * \param size                      Desired slot size.
     * \returns                         Geometry of the free slot of desired size whose upper-left corner 
     *                                  is closest (in L_inf sense) to the given position.
     */
    QRect closestFreeSlot(const QPoint &pos, const QSize &size) const;

signals:
    void entityAdded(QnDisplayEntity *entity);
    void entityAboutToBeRemoved(QnDisplayEntity *entity);

private:
    void moveEntityInternal(QnDisplayEntity *entity, const QRect &geometry);

private:
    QSet<QnDisplayEntity *> m_entities;
    QnMatrixMap<QnDisplayEntity *> m_entityMap;
    QnRectSet m_rectSet;
};

#endif // QN_DISPLAY_MODEL_H
