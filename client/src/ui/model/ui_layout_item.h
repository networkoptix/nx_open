#ifndef QN_UI_LAYOUT_ITEM_H
#define QN_UI_LAYOUT_ITEM_H

#include <QObject>
#include <QScopedPointer>
#include <core/resource/resource_consumer.h>

class QnUiLayout;
class QnMediaResource;
class QnAbstractArchiveReader;
class QnAbstractMediaStreamDataProvider;
class QnAbstractStreamDataProvider;
class CLCamDisplay;

/**
 * Single ui layout item. Video, image, folder, or anything else.
 */
class QnUiLayoutItem: public QObject, protected QnResourceConsumer {
    Q_OBJECT;
    Q_FLAGS(ItemFlag ItemFlags);
public:
    enum ItemFlag {
        Pinned = 0x1  /**< Item is pinned to the grid. Items are pinned by default. */
    };
    Q_DECLARE_FLAGS(ItemFlags, ItemFlag)

    /**
     * Constructor.
     * 
     * \param resource                  Resource that this item is associated with. Must not be NULL.
     * \param parent                    Parent of this object.                
     */
    QnUiLayoutItem(const QnResourcePtr &resource, QObject *parent = NULL);

    /**
     * Virtual destructor.
     */
    virtual ~QnUiLayoutItem();

    /**
     * \returns                         Layout that this item belongs to, if any.
     */
    QnUiLayout *layout() const {
        return m_layout;
    }

    /**
     * \returns                         Resource associated with this item.
     */
    QnResource *resource() const;

    /**
     * \returns                         Media resource associated with this item, if any.
     */
    QnMediaResource *mediaResource() const;

    /**
     * \returns                         Data provider associated with this item.
     */
    QnAbstractStreamDataProvider *dataProvider() const {
        return m_dataProvider;
    }

    /**
     * \returns                         Media data provider associated with this item, if any.
     */
    QnAbstractMediaStreamDataProvider *mediaProvider() const {
        return m_mediaProvider;
    }

    /**
     * \returns                         Camera display associated with this item, if any.
     */
    CLCamDisplay *camDisplay() const {
        return m_camDisplay.data();
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

    /**
     * \returns                         Length of this item, in microseconds. If the length is not defined, returns -1.
     */
    qint64 lengthUSec() const;

    /**
     * \returns                         Current time of this item, in microseconds. If the time is not defined, returns -1.
     */
    qint64 currentTimeUSec() const;

    /**
     * \param usec                      New current time for this item, in microseconds.
     */
    void setCurrentTimeUSec(qint64 usec) const;

    void play();

    void pause();

signals:
    void geometryChanged(const QRect &oldGeometry, const QRect &newGeometry);
    void geometryDeltaChanged(const QRectF &oldGeometryDelta, const QRectF &newGeometryDelta);
    void flagsChanged(ItemFlags oldFlags, ItemFlags newFlags);
    void rotationChanged(qreal oldRotation, qreal newRotation);

protected:
    /**
     * Ensures that this item does not belong to any layout.
     */
    void ensureRemoved();

    void setGeometryInternal(const QRect &geometry);
    
    void setFlagInternal(ItemFlag flag, bool value);

    virtual void beforeDisconnectFromResource() override;

    virtual void disconnectFromResource() override;

private:
    friend class QnUiLayout;

    /** Layout that this item belongs to. */
    QnUiLayout *m_layout;

    /** Grid-relative geometry of an item, in grid cells. */
    QRect m_geometry;

    /** Grid-relative geometry delta of an item, in grid cells. Meaningful for unpinned items only. */
    QRectF m_geometryDelta;

    /** Item flags. */
    ItemFlags m_flags;

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

Q_DECLARE_OPERATORS_FOR_FLAGS(QnUiLayoutItem::ItemFlags);

#endif // QN_UI_LAYOUT_ITEM_H
