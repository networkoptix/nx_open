#ifndef QN_WORKBENCH_ITEM_H
#define QN_WORKBENCH_ITEM_H

#include <QObject>
#include <QRect>
#include <QUuid>
#include <QScopedPointer>

class QnWorkbenchLayout;
class QnLayoutItemData;

/**
 * Layout item. Video, image, folder, or anything else.
 */
class QnWorkbenchItem : public QObject
{
    Q_OBJECT
    Q_FLAGS(ItemFlag ItemFlags)
    Q_PROPERTY(QString resourceUid READ resourceUid)
    Q_PROPERTY(QUuid uuid READ uuid)
    Q_PROPERTY(qreal rotation READ rotation WRITE setRotation)

public:
    enum ItemFlag {
        Pinned = 0x1,                       /**< Item is pinned to the grid. Items are not pinned by default. */
        PendingGeometryAdjustment = 0x2     /**< Geometry adjustment is pending. 
                                             * Center of item's combined geometry defines desired position. 
                                             * If item's rect is invalid, but not empty (width or height are negative), then any position is OK. */
    };
    Q_DECLARE_FLAGS(ItemFlags, ItemFlag)

    /**
     * Constructor.
     *
     * \param resource                  Unique identifier of the resource associated with this item.
     * \param uuid                      Universally unique identifier of this item.
     * \param parent                    Parent of this object.
     */
    QnWorkbenchItem(const QString &resourceUid, const QUuid &uuid, QObject *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~QnWorkbenchItem();

    /**
     * Load from QnLayoutData.
     *
     * Note that this function may partially fail. It does not roll back the
     * changes that were made before the failure.
     *
     * \param itemData                  Data to load from.
     * \returns                         Whether all fields were successfully loaded.
     */
    bool load(const QnLayoutItemData &itemData);

    /**
     * \param[out] itemData             Data to save to.
     */
    void save(QnLayoutItemData &itemData) const;

    /**
     * \returns                         Layout that this item belongs to, if any.
     */
    QnWorkbenchLayout *layout() const {
        return m_layout;
    }

    /**
     * \returns                         Unique identifier of the resource associated with this item.
     */
    const QString &resourceUid() const {
        return m_resourceUid;
    }

    /**
     * \returns                         Universally unique identifier of this item. 
     */
    const QUuid &uuid() const {
        return m_uuid;
    }

    /**
     * \returns                         Geometry of this item, in grid coordinates.
     */
    const QRect &geometry() const {
        return m_geometry;
    }

    /**
     * Note that this function may fail if the following conditions are met:
     * <ul>
     * <li>This item does not belong to a layout.</li>
     * <li>This item is pinned.</li>
     * <li>Given geometry is already occupied by some other item.</li>
     * </ul>
     *
     * If you want to have more control on how items are moved, use the
     * functions provided by the layout.
     *
     * \param geometry                  New geometry for this item.
     * \returns                         Whether the geometry was successfully changed.
     */
    bool setGeometry(const QRect &geometry);

    /**
     * \returns                         Geometry delta of this item, in grid coordinates.
     */
    const QRectF &geometryDelta() const {
        return m_geometryDelta;
    }

    /**
     * Note that this function will fail if this item is pinned.
     *
     * \param geometryDelta             New geometry delta for this item.
     * \returns                         Whether the geometry delta was successfully changed.
     */
    bool setGeometryDelta(const QRectF &geometryDelta);

    /**
     * \param combinedGeometry          New combined geometry for this item.
     *                                  New geometry and geometry delta will be calculated based on this value.
     * \returns                         Whether the combined geometry was successfully changed.
     */
    bool setCombinedGeometry(const QRectF &combinedGeometry);

    /**
     * \returns                         Combined geometry of this item.
     */
    QRectF combinedGeometry() const;

    /**
     * \returns                         Flags of this item.
     */
    ItemFlags flags() const {
        return m_flags;
    }

    /**
     * Note that this function may partially fail. It does not roll back
     * the changes that were made before the failure.
     *
     * \param flags                     New flags for this item.
     * \returns                         Whether the flags were successfully set.
     */
    bool setFlags(ItemFlags flags);

    /**
     * \param flag                      Flag to check.
     * \returns                         Whether given flag is set.
     */
    bool checkFlag(ItemFlag flag) const {
        return m_flags & flag;
    }

    /**
     * Note that this function will fail when trying to pin an item into a
     * position that is already occupied.
     *
     * \param flag                      Flag to set.
     * \param value                     New value for the flag.
     * \returns                         Whether the flag value was changed successfully.
     */
    bool setFlag(ItemFlag flag, bool value = true);

    /**
     * \returns                         Whether this item is pinned.
     */
    bool isPinned() const {
        return checkFlag(Pinned);
    }

    /**
     * \returns                         Rotation angle of this item.
     */
    qreal rotation() const {
        return m_rotation;
    }

    /**
     * \param degrees                   New rotation value for this item, in degrees.
     */
    void setRotation(qreal rotation);

    bool setPinned(bool value) { 
        return setFlag(Pinned, value); 
    }

    bool togglePinned() { 
        return setPinned(!isPinned()); 
    }

    void adjustGeometry();

    void adjustGeometry(const QPointF &desiredPosition);

signals:
    void geometryChanged();
    void geometryDeltaChanged();
    void flagChanged(QnWorkbenchItem::ItemFlag flag, bool value);
    void rotationChanged();

protected:
    void setGeometryInternal(const QRect &geometry);

    void setFlagInternal(ItemFlag flag, bool value);

private:
    friend class QnWorkbenchLayout;

    /** Layout that this item belongs to. */
    QnWorkbenchLayout *m_layout;

    /** Unique identifier of the resource associated with this item. */
    QString m_resourceUid;

    /** Universal unique identifier of this item. */
    QUuid m_uuid;

    /** Grid-relative geometry of an item, in grid cells. */
    QRect m_geometry;

    /** Grid-relative geometry delta of an item, in grid cells. Meaningful for unpinned items only. */
    QRectF m_geometryDelta;

    /** Item flags. */
    ItemFlags m_flags;

    /** Rotation, in degrees. */
    qreal m_rotation;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnWorkbenchItem::ItemFlags)

#endif // QN_WORKBENCH_ITEM_H
