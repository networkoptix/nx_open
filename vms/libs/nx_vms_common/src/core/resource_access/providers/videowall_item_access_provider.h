// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource_access/providers/base_resource_access_provider.h>

class QnLayoutItemAggregator;

namespace nx::core::access {

/**
 * Handles access to layouts, cameras and web pages, placed on videowall.
 */
class NX_VMS_COMMON_API VideoWallItemAccessProvider: public BaseResourceAccessProvider
{
    using base_type = BaseResourceAccessProvider;

public:
    VideoWallItemAccessProvider(
        Mode mode,
        nx::vms::common::SystemContext* context,
        QObject* parent = nullptr);
    virtual ~VideoWallItemAccessProvider();

protected:
    virtual Source baseSource() const override;

    virtual bool calculateAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource,
        nx::vms::api::GlobalPermissions globalPermissions) const override;

    virtual void fillProviders(
        const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource,
        QnResourceList& providers) const override;

    virtual void handleResourceAdded(const QnResourcePtr& resource) override;
    virtual void handleResourceRemoved(const QnResourcePtr& resource) override;

    virtual void afterUpdate() override;

private:
    void handleVideoWallAdded(const QnVideoWallResourcePtr& videoWall);
    void updateAccessToLayout(const QnLayoutResourcePtr& layout);
    void handleItemAdded(const QnUuid& resourceId);
    void handleItemRemoved(const QnUuid& resourceId);
    void handleOwnAccessRightsChanged(const QSet<QnUuid>& subjectIds);
    QnLayoutResourceList getLayoutsForVideoWall(const QnVideoWallResourcePtr& videoWall) const;

    void handleVideowallItemAdded(const QnVideoWallResourcePtr& videowall,
        const QnVideoWallItem& item);
    void handleVideowallItemChanged(const QnVideoWallResourcePtr& videowall,
        const QnVideoWallItem& item,
        const QnVideoWallItem& oldItem);
    void handleVideowallItemRemoved(const QnVideoWallResourcePtr& videowall,
        const QnVideoWallItem &item);

private:
    // Only resources with valid id can be accessible through layouts.
    QScopedPointer<QnLayoutItemAggregator> m_itemAggregator;
};

} // namespace nx::core::access
