#pragma once

#include <core/resource_access/providers/base_resource_access_provider.h>

class QnLayoutItemAggregator;

/** Handles access to layouts, cameras and web pages, placed on videowall. */
class QnVideoWallItemAccessProvider: public QnBaseResourceAccessProvider
{
    using base_type = QnBaseResourceAccessProvider;

public:
    QnVideoWallItemAccessProvider(Mode mode, QObject* parent = nullptr);
    virtual ~QnVideoWallItemAccessProvider();

protected:
    virtual Source baseSource() const override;

    virtual bool calculateAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const override;

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
    QnLayoutResourceList getLayoutsForVideoWall(const QnVideoWallResourcePtr& videoWall) const;

    void handleVideowallItemAdded(const QnVideoWallResourcePtr& videowall,
        const QnVideoWallItem& item);
    void handleVideowallItemChanged(const QnVideoWallResourcePtr& videowall,
        const QnVideoWallItem& item,
        const QnVideoWallItem& oldItem);
    void handleVideowallItemRemoved(const QnVideoWallResourcePtr& videowall,
        const QnVideoWallItem &item);

private:
    /* Only resources with valid id can be accessible through layouts. */
    QScopedPointer<QnLayoutItemAggregator> m_itemAggregator;
};
