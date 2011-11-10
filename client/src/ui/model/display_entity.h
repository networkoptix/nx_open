#ifndef QN_DISPLAY_ENTITY_H
#define QN_DISPLAY_ENTITY_H

#include <QObject>
#include <QScopedPointer>
#include <core/resource/resource_consumer.h>

class QnDisplayModel;
class QnMediaResource;
class QnAbstractArchiveReader;
class QnAbstractMediaStreamDataProvider;
class QnAbstractStreamDataProvider;
class CLCamDisplay;

/**
 * Single display entity. Video, image, folder, or anything else.
 */
class QnDisplayEntity: public QObject, protected QnResourceConsumer {
    Q_OBJECT;
    Q_FLAGS(EntityFlag EntityFlags);
public:
    enum EntityFlag {
        Pinned = 0x1  /**< Entity is pinned to the grid. Entities are pinned by default. */
    };
    Q_DECLARE_FLAGS(EntityFlags, EntityFlag)

    /**
     * Constructor.
     * 
     * \param resource                  Resource that this entity is associated with. Must not be NULL.
     * \param parent                    Parent of this object.                
     */
    QnDisplayEntity(const QnResourcePtr &resource, QObject *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~QnDisplayEntity();

    /**
     * \returns                         Model that this entity belongs to, if any.
     */
    QnDisplayModel *model() const {
        return m_model;
    }

    /**
     * \returns                         Resource associated with this entity.
     */
    QnResource *resource() const;

    /**
     * \returns                         Media resource associated with this entity, if any.
     */
    QnMediaResource *mediaResource() const;

    /**
     * \returns                         Data provider associated with this entity.
     */
    QnAbstractStreamDataProvider *dataProvider() const {
        return m_dataProvider;
    }

    /**
     * \returns                         Media data provider associated with this entity, if any.
     */
    QnAbstractMediaStreamDataProvider *mediaProvider() const {
        return m_mediaProvider;
    }

    /**
     * \returns                         Camera display associated with this entity, if any.
     */
    CLCamDisplay *camDisplay() const {
        return m_camDisplay.data();
    }

    /**
     * \returns                         Geometry of this entity, in grid coordinates.
     */
    const QRect &geometry() const {
        return m_geometry;
    }

    /**
     * Note that this function may fail if the following conditions are met:
     * <ul>
     * <li>This entity belongs to a model.</li>
     * <li>This entity is pinned.</li>
     * <li>Given geometry is already occupied by some other entity.</li>
     * </ul>
     * 
     * If you want to have more control on how entities are moved, use the
     * function provided by the model.
     *
     * \param geometry                  New geometry for this entity.
     * \returns                         Whether the geometry was successfully changed.
     */
    bool setGeometry(const QRect &geometry);

    /**
     * \returns                         Geometry delta of this entity, in grid coordinates.
     */
    const QRectF &geometryDelta() const {
        return m_geometryDelta;
    }

    /**
     * Note that this function will fail if this entity is pinned.
     *
     * \param geometryDelta             New geometry delta for this entity.
     * \returns                         Whether the geometry delta was successfully changed.
     */
    bool setGeometryDelta(const QRectF &geometryDelta);

    /**
     * \returns                         Flags of this entity.
     */
    EntityFlags flags() const {
        return m_flags;
    }

    /**
     * \param flag                      Flag to check.
     * \returns                         Whether given flag is set.
     */
    bool checkFlag(EntityFlag flag) const {
        return m_flags & flag;
    }

    /**
     * \returns                         Whether this entity is pinned.
     */
    bool isPinned() const {
        return checkFlag(Pinned);
    }

    /**
     * Note that this function will fail when trying to pin an entity into a 
     * position that is already occupied.
     *
     * \param flag                      Flag to set.
     * \param value                     New value for the flag.
     * \returns                         Whether the flag value was changed successfully.
     */
    bool setFlag(EntityFlag flag, bool value = true);

    /**
     * \returns                         Rotation angle of this entity.
     */
    qreal rotation() const {
        return m_rotation;
    }

    /**
     * \param degrees                   New rotation value for this entity, in degrees.
     */
    void setRotation(qreal rotation);

    /**
     * \returns                         Length of this display entity, in microseconds. If the length is not defined, returns -1.
     */
    qint64 lengthUSec() const;

    /**
     * \returns                         Current time of this display entity, in microseconds. If the time is not defined, returns -1.
     */
    qint64 currentTimeUSec() const;

    /**
     * \param usec                      New current time for this display entity, in microseconds.
     */
    void setCurrentTimeUSec(qint64 usec) const;

    void play();

    void pause();

signals:
    void geometryChanged(const QRect &oldGeometry, const QRect &newGeometry);
    void geometryDeltaChanged(const QRectF &oldGeometryDelta, const QRectF &newGeometryDelta);
    void flagsChanged(EntityFlags oldFlags, EntityFlags newFlags);
    void rotationChanged(qreal oldRotation, qreal newRotation);

protected:
    /**
     * Ensures that this entity does not belong to any model.
     */
    void ensureRemoved();

    void setGeometryInternal(const QRect &geometry);
    
    void setFlagInternal(EntityFlag flag, bool value);

    virtual void beforeDisconnectFromResource() override;

    virtual void disconnectFromResource() override;

private:
    friend class QnDisplayModel;

    /** Model that this entity belongs to. */
    QnDisplayModel *m_model;

    /** Grid-relative geometry of an entity, in grid cells. */
    QRect m_geometry;

    /** Grid-relative geometry delta of an entity, in grid cells. Meaningful for unpinned entities only. */
    QRectF m_geometryDelta;

    /** Entity flags. */
    EntityFlags m_flags;

    /** Rotation, in degrees. */
    qreal m_rotation;

    /** Data provider for the associated resource. */
    QnAbstractStreamDataProvider *m_dataProvider;

    /** Media data provider, if any. */
    QnAbstractMediaStreamDataProvider *m_mediaProvider;

    /** Archive data provider, if any. */
    QnAbstractArchiveReader *m_archiveProvider;

    /** Camera display, if any. */
    QScopedPointer<CLCamDisplay> m_camDisplay;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnDisplayEntity::EntityFlags);

#endif // QN_DISPLAY_ENTITY_H
