#pragma once

#include <core/resource_access/providers/base_resource_access_provider.h>

class QnImplicitServerMonitorAccessProvider: public QnBaseResourceAccessProvider
{
    using base_type = QnBaseResourceAccessProvider;

public:
    QnImplicitServerMonitorAccessProvider(Mode mode, QObject* parent = nullptr);
    virtual ~QnImplicitServerMonitorAccessProvider() override;

protected:
    virtual Source baseSource() const override;
    virtual bool calculateAccess(
        const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource,
        nx::vms::api::GlobalPermissions globalPermissions) const override;
};
