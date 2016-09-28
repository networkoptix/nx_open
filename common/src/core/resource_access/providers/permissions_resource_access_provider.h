#pragma once

#include <core/resource_access/providers/base_resource_access_provider.h>

/** Handles access via global permissions. */
class QnPermissionsResourceAccessProvider: public QnBaseResourceAccessProvider
{
    using base_type = QnBaseResourceAccessProvider;
public:
    QnPermissionsResourceAccessProvider(QObject* parent = nullptr);
    virtual ~QnPermissionsResourceAccessProvider();

protected:
    virtual bool calculateAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const override;
private:
    bool hasAccessToDesktopCamera(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const;

};
