#pragma once

#include <core/resource_access/providers/base_resource_access_provider.h>

/** Handles access to layouts, cameras and web pages, placed on videowall. */
class QnVideoWallItemAccessProvider: public QnBaseResourceAccessProvider
{
    using base_type = QnBaseResourceAccessProvider;
public:
    QnVideoWallItemAccessProvider(QObject* parent = nullptr);
    virtual ~QnVideoWallItemAccessProvider();

protected:
    virtual Source baseSource() const override;

    virtual bool calculateAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const override;

    virtual void handleResourceAdded(const QnResourcePtr& resource) override;
    virtual void handleResourceRemoved(const QnResourcePtr& resource) override;

private:
    void handleVideoWallItemAdded(const QnVideoWallResourcePtr& resource,
        const QnVideoWallItem &item);
    void handleVideoWallItemChanged(const QnVideoWallResourcePtr& resource,
        const QnVideoWallItem& oldItem, const QnVideoWallItem& item);
    void handleVideoWallItemRemoved(const QnVideoWallResourcePtr& resource,
        const QnVideoWallItem &item);

    void updateByLayoutId(const QnUuid& id);
    void updateAccessToVideoWallItems(const QnVideoWallResourcePtr& videoWall);

    QSet<QnUuid> accessibleLayouts() const;
};
