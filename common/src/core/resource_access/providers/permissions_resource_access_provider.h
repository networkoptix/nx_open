#pragma once

#include <core/resource_access/providers/abstract_resource_access_provider.h>

/** Handles access via global permissions. */
class QnPermissionsResourceAccessProvider: public QnAbstractResourceAccessProvider
{
    Q_OBJECT
    using base_type = QnAbstractResourceAccessProvider;
public:
    QnPermissionsResourceAccessProvider(QObject* parent = nullptr);
    virtual ~QnPermissionsResourceAccessProvider();

    virtual bool hasAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const override;

private:
    bool hasAccessToDesktopCamera(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const;
};
