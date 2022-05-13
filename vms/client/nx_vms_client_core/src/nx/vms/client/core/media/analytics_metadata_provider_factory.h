// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <functional>
#include <memory>

#include <QtCore/QHash>

#include <core/resource/resource_fwd.h>
#include "abstract_analytics_metadata_provider.h"

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API AbstractAnalyticsMetadataProviderFactory
{
public:
    virtual ~AbstractAnalyticsMetadataProviderFactory();

    virtual bool supportsAnalytics(const QnResourcePtr& resource) const = 0;
    virtual AbstractAnalyticsMetadataProviderPtr createMetadataProvider(
        const QnResourcePtr& resource) const = 0;
};

class NX_VMS_CLIENT_CORE_API AnalyticsMetadataProviderFactory:
    public AbstractAnalyticsMetadataProviderFactory
{
public:
    /**
     * @return true if one of the registered factories reports it supports analytics for the
     * given resource. The registered factories are checked in the same order as they was
     * registered.
     */
    virtual bool supportsAnalytics(const QnResourcePtr& resource) const override;

    /**
     * @return metadata provider for the given resource or null if none of the registered factories
     * supports analytics for the given resource.
     */
    virtual AbstractAnalyticsMetadataProviderPtr createMetadataProvider(
        const QnResourcePtr& resource) const override;

    /**
     * Register metadata factory.
     *
     * Note AnalyticsMetadataProviderFactory takes ownership of the given factory and will delete
     * it in the destructor.
     */
    void registerMetadataFactory(
        const QString& type,
        AbstractAnalyticsMetadataProviderFactory* factory);

private:
    QString analyticsTypeForResource(const QnResourcePtr& resource) const;

private:
    using Factory = QPair<QString, QSharedPointer<AbstractAnalyticsMetadataProviderFactory>>;
    QVector<Factory> m_factories;
};

} // namespace nx::vms::client::core
