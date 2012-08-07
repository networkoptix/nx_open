#ifndef QN_WORKBENCH_ITEM_H
#define QN_WORKBENCH_ITEM_H

#include <QtCore/QObject>
#include <QtCore/QRect>
#include <QtCore/QUuid>
#include <QtCore/QScopedPointer>

#include "workbench_globals.h"

class QnWorkbenchLayout;
class QnLayoutItemData;

/**
 * Layout item. Video, image, folder, or anything else.
 */
class QnWorkbenchItem : public QObject {
    Q_OBJECT;
    Q_PROPERTY(QString resourceUid READ resourceUid);
    Q_PROPERTY(QUuid uuid READ uuid);
    Q_PROPERTY(qreal rotation READ rotation WRITE setRotation);

public:
    /**
     * Constructor.
     *
     * \param resource                  Unique identifier of the resource associated with this item.
     * \param uuid                      Universally unique identifier of this item.
     * \param parent                    Parent of this object.
     */
    QnWorkbenchItem(const QString &resourceUid, const QUuid &uuid, QObject *parent = NULL);

    /**
     * Constructor.
     * 
     * \param data                      Item data to create an item from.
     * \param parent                    Parent of this object.
     */
    QnWorkbenchItem(const QnLayoutItemData &data, QObject *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~QnWorkbenchItem();

    /**
     * \returns                         Layout item data of this workbench item.
     */
    QnLayoutItemData data() const;

    /**
     * Update from QnLayoutData.
     *
     * Note that this function may partially fail. It does not roll back the
     * changes that were made before the failure.
     *
     * \param data                      Data to update from.
     * \returns                         Whether all fields were successfully updated.
     */
    bool update(const QnLayoutItemData &data);

    /**
     * \param[out] data                 Data to submit to.
     */
    void submit(QnLayoutItemData &data) const;

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
    Qn::ItemFlags flags() const {
        return m_flags;
    }

    /**
     * Note that this function may partially fail. It does not roll back
     * the changes that were made before the failure.
     *
     * \param flags                     New flags for this item.
     * \returns                         Whether the flags were successfully set.
     */
    bool setFlags(Qn::ItemFlags flags);

    /**
     * \param flag                      Flag to check.
     * \returns                         Whether given flag is set.
     */
    bool checkFlag(Qn::ItemFlag flag) const {
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
    bool setFlag(Qn::ItemFlag flag, bool value = true);

    /**
     * \returns                         Whether this item is pinned.
     */
    bool isPinned() const {
        return checkFlag(Qn::Pinned);
    }

    /**
     * \returns                         Rotation angle of this item, in degrees.
     */
    qreal rotation() const {
        return m_rotation;
    }

    /**
     * \param degrees                   New rotation value for this item, in degrees.
     */
    void setRotation(qreal rotation);

    bool setPinned(bool value) { 
        return setFlag(Qn::Pinned, value); 
    }

    bool togglePinned() { 
        return setPinned(!isPinned()); 
    }

    /**
     * Marks this item as waiting for geometry adjustment. It will be placed
     * at the best slot available when its layout becomes active.
     */
    void adjustGeometry();

    /**
     * Marks this item as waiting for geometry adjustment. It will be placed 
     * at the best slot that is close to given position when its layout becomes active.
     * 
     * \param desiredPosition           Position in grid coordinates. This item
     *                                  will be placed as close to this position as possible.
     */
    void adjustGeometry(const QPointF &desiredPosition);

signals:
    void geometryChanged();
    void geometryDeltaChanged();
    void flagChanged(Qn::ItemFlag flag, bool value);
    void rotationChanged();

protected:
    void setGeometryInternal(const QRect &geometry);

    void setFlagInternal(Qn::ItemFlag flag, bool value);

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
    Qn::ItemFlags m_flags;

    /** Rotation, in degrees. */
    qreal m_rotation;
};

#endif // QN_WORKBENCH_ITEM_H
