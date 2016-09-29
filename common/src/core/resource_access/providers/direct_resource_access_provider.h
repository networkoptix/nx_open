#pragma once

#include <core/resource_access/providers/base_resource_access_provider.h>

/** Handles access via 'shared resources' db table. */
class QnDirectResourceAccessProvider: public QnBaseResourceAccessProvider
{
    using base_type = QnBaseResourceAccessProvider;
public:
    QnDirectResourceAccessProvider(QObject* parent = nullptr);
    virtual ~QnDirectResourceAccessProvider();

protected:
    virtual Source baseSource() const override;

    virtual bool calculateAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const override;

    virtual void handleResourceAdded(const QnResourcePtr& resource);

private:
    void handleSharedResourcesChanged(const QnResourceAccessSubject& subject,
        const QSet<QnUuid>& resourceIds);
};
