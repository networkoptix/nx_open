// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <core/resource_access/providers/base_resource_access_provider.h>

class QnLayoutItemAggregator;
using QnLayoutItemAggregatorPtr = QSharedPointer<QnLayoutItemAggregator>;

namespace nx::core::access {

/**
 * Handles access to cameras and web pages, placed on shared layouts.
 */
class NX_VMS_COMMON_API SharedLayoutItemAccessProvider: public BaseResourceAccessProvider
{
    using base_type = BaseResourceAccessProvider;

public:
    SharedLayoutItemAccessProvider(
        Mode mode,
        nx::vms::common::SystemContext* context,
        QObject* parent = nullptr);
    virtual ~SharedLayoutItemAccessProvider();

protected:
    virtual Source baseSource() const override;

    virtual bool calculateAccess(const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource,
        nx::vms::api::GlobalPermissions globalPermissions) const override;

    virtual void fillProviders(
        const QnResourceAccessSubject& subject,
        const QnResourcePtr& resource,
        QnResourceList& providers) const override;

    virtual void handleResourceAdded(const QnResourcePtr& resource) override;
    virtual void handleResourceRemoved(const QnResourcePtr& resource) override;

    virtual void handleSubjectAdded(const QnResourceAccessSubject& subject) override;
    virtual void handleSubjectRemoved(const QnResourceAccessSubject& subject) override;

private:
    void handleSharedResourcesChanged(const QnResourceAccessSubject& subject,
        const QSet<QnUuid>& oldValues, const QSet<QnUuid>& newValues);

    void updateAccessToLayout(const QnLayoutResourcePtr& layout);

    QnLayoutItemAggregatorPtr findAggregatorForSubject(const QnResourceAccessSubject& subject);
    QnLayoutItemAggregatorPtr ensureAggregatorForSubject(const QnResourceAccessSubject& subject);

private:
    QHash<QnUuid, QnLayoutItemAggregatorPtr> m_aggregatorsBySubject;
};

} // namespace nx::core::access
