#pragma once

#include <core/resource_access/providers/base_resource_access_provider.h>

/** Handles access to cameras and web pages, placed on shared layouts. */
class QnSharedLayoutItemAccessProvider: public QnBaseResourceAccessProvider
{
    using base_type = QnBaseResourceAccessProvider;

public:
    QnSharedLayoutItemAccessProvider(QObject* parent = nullptr);
    virtual ~QnSharedLayoutItemAccessProvider();

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

private:
    void handleSharedResourcesChanged(const QnResourceAccessSubject& subject,
        const QSet<QnUuid>& oldValues, const QSet<QnUuid>& newValues);

    void updateAccessToLayoutItems(const QnLayoutResourcePtr& layout);
};