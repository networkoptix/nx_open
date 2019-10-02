#pragma once

#include <core/resource_access/providers/base_resource_access_provider.h>

class QnParentServerMonitorAccessProvider: public QnBaseResourceAccessProvider
{
    using base_type = QnBaseResourceAccessProvider;
    using AccessProviderList = QList<QnAbstractResourceAccessProvider*>;

public:
    QnParentServerMonitorAccessProvider(Mode mode, const AccessProviderList& sourceProviders,
        QObject* parent = nullptr);
    virtual ~QnParentServerMonitorAccessProvider() override;

protected:
    virtual Source baseSource() const override;

    virtual bool calculateAccess(
        const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource,
        nx::vms::api::GlobalPermissions globalPermissions) const override;

    virtual void fillProviders(
        const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource,
        QnResourceList& providers) const override;

    virtual void handleResourceAdded(const QnResourcePtr& resource) override;
    virtual void handleResourceRemoved(const QnResourcePtr& resource) override;

private:
    void onSourceAccessChanged(
        const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource,
        Source value);
    void onDeviceParentIdChanged(const QnResourcePtr& resource);

    bool hasAccessBySourceProviders(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource) const;

    void addToCameraIndex(const QnResourcePtr& resource);
    void removeFromCameraIndex(const QnResourcePtr& resource);

private:
    AccessProviderList m_sourceProviders;
    QHash<QnUuid, QSet<QnResourcePtr>> m_devicesByServerId;
};
