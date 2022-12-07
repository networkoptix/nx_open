// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>
#include <QtCore/QObject>
#include <QtCore/QRect>
#include <QtCore/QScopedPointer>
#include <QtCore/QVariant>
#include <QtGui/QColor>

#include <client/client_globals.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/dewarping_data.h>
#include <nx/vms/api/data/image_correction_data.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>

class QnWorkbenchLayout;

/**
 * Layout item. Video, image, server, or anything else.
 */
class QnWorkbenchItem: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QnUuid uuid READ uuid)
    Q_PROPERTY(qreal rotation READ rotation WRITE setRotation)

public:
    /**
     * Constructor.
     *
     * \param resource                  Resource associated with this item.
     * \param uuid                      Universally unique identifier of this item.
     * \param parent                    Parent of this object.
     */
    QnWorkbenchItem(const QnResourcePtr& resource, const QnUuid& uuid, QObject* parent = nullptr);

    /**
     * Constructor.
     *
     * \param data                      Item data to create an item from.
     * \param parent                    Parent of this object.
     */
    QnWorkbenchItem(const QnResourcePtr& resource, const QnLayoutItemData& data, QObject* parent = nullptr);

    /**
     * Virtual destructor.
     */
    virtual ~QnWorkbenchItem();

    /**
     * Set layout this item belongs to. Value is set after item is completely initialized and reset
     * just before destroying.
     */
    void setLayout(QnWorkbenchLayout* layout);

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
    QnWorkbenchLayout *layout() const
    {
        return m_layout;
    }

    /**
     * \returns                         Resource associated with this item.
     */
    QnResourcePtr resource() const;

    /**
     * \returns                         Universally unique identifier of this item.
     */
    const QnUuid &uuid() const
    {
        return m_uuid;
    }

    /**
     * \returns                         Geometry of this item, in grid coordinates.
     */
    const QRect &geometry() const
    {
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
    const QRectF &geometryDelta() const
    {
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
    Qn::ItemFlags flags() const
    {
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
    bool hasFlag(Qn::ItemFlag flag) const
    {
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
    bool isPinned() const
    {
        return hasFlag(Qn::Pinned);
    }

    /**
     * \param pinned                    New pinned state for this item.
     * \returns                         Whether the pinned state was successfully set.
     */
    bool setPinned(bool pinned)
    {
        return setFlag(Qn::Pinned, pinned);
    }

    /**
     * Toggles the current pinned state of this item.
     *
     * \returns                         Whether the pinned state was successfully changed.
     */
    bool togglePinned()
    {
        return setPinned(!isPinned());
    }

    /**
     * \returns                         Zoom rect of this item, in item-relative coordinates.
     */
    const QRectF &zoomRect() const
    {
        return m_zoomRect;
    }

    /**
     * Note that zoom rect will be used only if an appropriate zoom link is
     * created for this item in a layout.
     *
     * \param zoomRect                  New zoom rect for this item.
     */
    void setZoomRect(const QRectF &zoomRect);

    /**
     * \param                           New image enhancement params for this item.
     */
    void setImageEnhancement(const nx::vms::api::ImageCorrectionData& imageEnhancement);

    /**
     * \param                           New dewarping enhancement params for this item.
     */
    void setDewarpingParams(const nx::vms::api::dewarping::ViewData& params);


    const nx::vms::api::ImageCorrectionData& imageEnhancement() const
    {
        return m_imageEnhancement;
    }

    const nx::vms::api::dewarping::ViewData& dewarpingParams() const
    {
        return m_itemDewarpingParams;
    }

    QnWorkbenchItem *zoomTargetItem() const;

    /**
     * \returns                         Rotation angle of this item, in degrees.
     */
    qreal rotation() const
    {
        return m_rotation;
    }

    /**
     * \param degrees                   New rotation value for this item, in degrees.
     */
    void setRotation(qreal rotation);


    bool displayInfo() const;
    void setDisplayInfo(bool value);

    bool controlPtz() const;
    void setControlPtz(bool value);

    bool displayAnalyticsObjects() const;
    void setDisplayAnalyticsObjects(bool value);

    bool displayRoi() const;
    void setDisplayRoi(bool value);

    QColor frameDistinctionColor() const;
    void setFrameDistinctionColor(const QColor &value);

    /**
     * \param role                      Role to get data for.
     * \returns                         Data for the given role.
     */
    QVariant data(Qn::ItemDataRole role) const;

    /**
     * \param role                      Role to get data for.
     * \param defaultValue              Value to return if there is no stored data for the given role.
     * \returns                         Data for the given role.
     */
    template<class T>
    T data(Qn::ItemDataRole role, const T &defaultValue = T()) const
    {
        QVariant result = data(role);
        if (result.canConvert<T>())
        {
            return result.value<T>();
        }
        else
        {
            return defaultValue;
        }
    }

    template<class T>
    void setData(Qn::ItemDataRole role, const T &value)
    {
        setData(role, QVariant::fromValue<T>(value));
    }

    /**
     * \param role                      Role to set data for.
     * \param value                     New value for the given data role.
     */
    void setData(Qn::ItemDataRole role, const QVariant &value);

    static void sortByGeometryAndName(QList<QnWorkbenchItem*>& items);

signals:
    void geometryChanged();
    void geometryDeltaChanged();
    void flagChanged(Qn::ItemFlag flag, bool value);
    void zoomRectChanged();
    void imageEnhancementChanged();
    void dewarpingParamsChanged();
    void zoomTargetItemChanged();
    void rotationChanged();
    void displayInfoChanged();
    void controlPtzChanged();
    void displayAnalyticsObjectsChanged();
    void displayRoiChanged();
    void dataChanged(Qn::ItemDataRole role);

protected:
    void setGeometryInternal(const QRect &geometry);

    void setFlagInternal(Qn::ItemFlag flag, bool value);

private:
    friend class QnWorkbenchLayout;

    /** Layout that this item belongs to. */
    QnWorkbenchLayout* m_layout = nullptr;

    /** Universal unique identifier of this item. */
    const QnUuid m_uuid;

    const QnResourcePtr m_resource;

    /** Grid-relative geometry of an item, in grid cells. */
    QRect m_geometry;

    /** Grid-relative geometry delta of an item, in grid cells. Meaningful for unpinned items only. */
    QRectF m_geometryDelta;

    /** Item-relative rectangle that defines the portion of the item to be shown. */
    QRectF m_zoomRect;

    /** Item image enhancement params. */
    nx::vms::api::ImageCorrectionData m_imageEnhancement;

    /** Item flags. */
    Qn::ItemFlags m_flags;

    /** Rotation, in degrees. */
    qreal m_rotation = 0.0;

    /** Current item dewarping parameters. */
    nx::vms::api::dewarping::ViewData m_itemDewarpingParams;

    /** Should the info be always displayed on the item. */
    bool m_displayInfo = false;
    /** Whether PTZ control is enabled. */
    bool m_controlPtz = false;
    /** Should the detected analytics objects be always displayed on the item. */
    bool m_displayAnalyticsObjects = false;
    /** Should the regions of interest be always displayed on the item. */
    bool m_displayRoi = true;
    /** Base frame color */
    QColor m_frameDistinctionColor;
};
