#pragma once

#include <core/resource_access/providers/base_resource_access_provider.h>

/** Handles access to layouts, cameras and web pages, placed on videowall. */
class QnVideoWallResourceAccessProvider: public QnBaseResourceAccessProvider
{
    using base_type = QnBaseResourceAccessProvider;
public:
    QnVideoWallResourceAccessProvider(QObject* parent = nullptr);
    virtual ~QnVideoWallResourceAccessProvider();

protected:
    virtual bool calculateAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const override;

    virtual void handleResourceAdded(const QnResourcePtr& resource);

private:
    void handleVideoWallItemAdded(const QnVideoWallResourcePtr& resource,
        const QnVideoWallItem &item);
    void handleVideoWallItemChanged(const QnVideoWallResourcePtr& resource,
        const QnVideoWallItem& oldItem, const QnVideoWallItem& item);
    void handleVideoWallItemRemoved(const QnVideoWallResourcePtr& resource,
        const QnVideoWallItem &item);

    void updateByLayoutId(const QnUuid& id);

    QSet<QnUuid> accessibleLayouts() const;
};
