#pragma once

#include <core/resource_access/providers/base_resource_access_provider.h>

/** Provides access to directly shared resources. */
class QnSharedResourceAccessProvider: public QnBaseResourceAccessProvider
{
    using base_type = QnBaseResourceAccessProvider;

public:
    QnSharedResourceAccessProvider(Mode mode, QObject* parent = nullptr);
    virtual ~QnSharedResourceAccessProvider();

protected:
    virtual Source baseSource() const override;

    virtual bool calculateAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const override;

    virtual void handleResourceAdded(const QnResourcePtr& resource);

private:
    void handleSharedResourcesChanged(const QnResourceAccessSubject& subject,
        const QSet<QnUuid>& oldValues, const QSet<QnUuid>& newValues);
};
