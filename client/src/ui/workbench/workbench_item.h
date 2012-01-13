#ifndef QN_WORKBENCH_ITEM_H
#define QN_WORKBENCH_ITEM_H

#include <QtCore/QObject>
#include <QtCore/QRect>
#include <QtCore/QScopedPointer>

#include <core/resource/resource.h>
#include "core/resource/layout_data.h"

class QnWorkbenchLayout;

/**
 * Layout item model. Video, image, folder, or anything else.
 */
class QnWorkbenchItem : public QObject
{
    Q_OBJECT
    Q_FLAGS(ItemFlag ItemFlags)

public:
    enum ItemFlag {
        Pinned = 0x1  /**< Item is pinned to the grid. Items are pinned by default. */
    };
    Q_DECLARE_FLAGS(ItemFlags, ItemFlag)

    /**
     * Constructor.
     *
     * \param resource                  Resource associated with this item.
     * \param parent                    Parent of this object.
     */
    QnWorkbenchItem(const QnResourcePtr &resource, QObject *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~QnWorkbenchItem();

    /**
      * Load from QnLayoutData
      */
    void load(const QnLayoutItemData&);

    /**
     * \returns                         Layout that this item belongs to, if any.
     */
    QnWorkbenchLayout *layout() const {
        return m_layout;
    }

    /**
     * \returns                         Resource associated with this item.
     */
    const QnResourcePtr &resource() const {
        return m_resource;
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
     * <li>This item belongs to a model.</li>
     * <li>This item is pinned.</li>
     * <li>Given geometry is already occupied by some other item.</li>
     * </ul>
     *
     * If you want to have more control on how items are moved, use the
     * function provided by the layout.
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
     * \param flag                      Flag to check.
     * \returns                         Whether given flag is set.
     */
    bool checkFlag(ItemFlag flag) const {
        return m_flags & flag;
    }

    /**
     * \returns                         Whether this item is pinned.
     */
    bool isPinned() const {
        return checkFlag(Pinned);
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
     * \returns                         Rotation angle of this item.
     */
    qreal rotation() const {
        return m_rotation;
    }

    /**
     * \param degrees                   New rotation value for this item, in degrees.
     */
    void setRotation(qreal rotation);

signals:
    void geometryChanged();
    void geometryDeltaChanged();
    void flagsChanged();
    void rotationChanged();

protected:
    /**
     * Ensures that this item does not belong to any layout.
     */
    void ensureRemoved();

    void setGeometryInternal(const QRect &geometry);

    void setFlagInternal(ItemFlag flag, bool value);

private:
    friend class QnWorkbenchLayout;

    /** Layout that this item belongs to. */
    QnWorkbenchLayout *m_layout;

    /** Resource associated with this item. */
    QnResourcePtr m_resource;

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
