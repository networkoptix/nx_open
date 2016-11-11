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

    virtual void fillProviders(
        const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource,
        QnResourceList& providers) const override;

    virtual void handleResourceAdded(const QnResourcePtr& resource) override;
    virtual void handleResourceRemoved(const QnResourcePtr& resource) override;

    virtual void afterUpdate() override;

private:
    void handleVideoWallAdded(const QnVideoWallResourcePtr& videoWall);
    void handleLayoutAdded(const QnLayoutResourcePtr& layout);

    void updateByLayout(const QnLayoutResourcePtr& layout);

    void updateAccessibleLayouts();
    QSet<QnLayoutResourcePtr> accessibleLayouts() const;

private:
    QSet<QnLayoutResourcePtr> m_accessibleLayouts;
};
