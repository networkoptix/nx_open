// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>

#include <client/client_globals.h>
#include <core/resource/layout_resource.h>
#include <nx/utils/scoped_connections.h>
#include <recording/time_period.h>

#include "resource_fwd.h"

namespace nx::vms::client::desktop {

class NX_VMS_CLIENT_DESKTOP_API LayoutResource: public QnLayoutResource
{
    Q_OBJECT

public:
    using DataHash = QHash<Qn::ItemDataRole, QVariant>;
    using ItemsRemapHash = QHash<QnUuid, QnUuid>;

    static qreal cellSpacingValue(Qn::CellSpacing spacing);

    LayoutResource();

    void setPredefinedCellSpacing(Qn::CellSpacing spacing);

    /**
     * Clone items of the current layout to the target layout. Ids of the layout items will be
     *     re-generated, zoom links correspondingly fixed.
     * @param target Target layout. Existing items will be removed (if any).
     * @param remapHash If passed, filled with map of new items ids by old items ids.
     */
    void cloneItems(LayoutResourcePtr target, ItemsRemapHash* remapHash = nullptr) const;

    /**
     * Clone current layout to the target one.
     * @param target Target layout. Existing items will be removed (if any).
     * @param remapHash If passed, filled with map of new items ids by old items ids.
     */
    void cloneTo(LayoutResourcePtr target, ItemsRemapHash* remapHash = nullptr) const;

    /**
     * Clone current layout to the new layout.
     * @param remapHash If passed, filled with map of new items ids by old items ids.
     * @return New layout (not added to any system context).
     */
    LayoutResourcePtr clone(ItemsRemapHash* remapHash = nullptr) const;

    /** Allowed time period range. Can be applied e.g for exported layout or for audit trail. */
    QnTimePeriod localRange() const;

    /** Set allowed time period range for the layout. */
    void setLocalRange(const QnTimePeriod& value);

    /** Runtime layout data. */
    DataHash data() const;

    /** Set runtime layout data. */
    void setData(const DataHash& data);

    /** Runtime layout data. */
    QVariant data(Qn::ItemDataRole role) const;

    /** Set runtime layout data. */
    void setData(Qn::ItemDataRole role, const QVariant &value);

    /** Runtime data per item. */
    QVariant itemData(const QnUuid& id, Qn::ItemDataRole role) const;

    /** Store runtime data. Emits itemDataChanged if data differs from existing. */
    void setItemData(const QnUuid& id, Qn::ItemDataRole role, const QVariant& data);

    /** Remove all runtime data for the given item. Does not emit signals. */
    void cleanupItemData(const QnUuid& id);

    /** Remove all runtime data for all items. Does not emit signals. */
    void cleanupItemData();

    /** Whether this layout is Preview Search layout. */
    bool isPreviewSearchLayout() const;

    /** Whether this layout is a Showreel review layout. */
    bool isShowreelReviewLayout() const;

    /** Whether this layout is a Video Wall review layout. */
    bool isVideoWallReviewLayout() const;

    /** Whether this Layout belongs to an Intercom Camera. Is valid for removed intercom. */
    bool isIntercomLayout() const;

    /**
     * Whether this Layout is a Cross-System Layout. Such layouts are stored in the Cloud and can
     * contain Cameras from different Systems. Cross-System Layouts are personal and cannot be
     * shared.
     **/
    bool isCrossSystem() const;

signals:
    void dataChanged(Qn::ItemDataRole role);
    void itemDataChanged(const QnUuid& id, Qn::ItemDataRole role, const QVariant& data);

protected:
    /** Virtual constructor for cloning. */
    virtual LayoutResourcePtr create() const;

    virtual void setSystemContext(nx::vms::common::SystemContext* systemContext) override;
    virtual void updateInternal(const QnResourcePtr& source, NotifierList& notifiers) override;

private:
    /** @return Whether data value was changed. */
    bool setItemDataUnderLock(const QnUuid& id, Qn::ItemDataRole role, const QVariant& data);

    void updateIsIntercomState();

private:
    QnTimePeriod m_localRange;
    DataHash m_data;
    QHash<QnUuid, DataHash> m_itemData;

    bool m_isIntercomLayout = false;

    nx::utils::ScopedConnection m_resourcePoolConnection;
};

} // namespace nx::vms::client::desktop
