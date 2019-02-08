#pragma once

#include <vector>

#include <QtCore/QJsonObject>

#include <core/resource/resource.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/api/analytics/plugin_manifest.h>

namespace nx::vms::common {

/**
 * VMS Resource corresponding to an Analytics Plugin which has been loaded at least once by any
 * Server in the System.
 */
class AnalyticsPluginResource: public QnResource
{
    using base_type = QnResource;

public:
    static QString kPluginManifestProperty;

    AnalyticsPluginResource(QnCommonModule* commonModule = nullptr);
    virtual ~AnalyticsPluginResource() override = default;

    api::analytics::PluginManifest manifest() const;
    void setManifest(const api::analytics::PluginManifest& manifest);

    AnalyticsEngineResourceList engines() const;
    bool hasDefaultEngine() const;
};

} // namespace nx::vms::common

Q_DECLARE_METATYPE(nx::vms::common::AnalyticsPluginResourcePtr)
Q_DECLARE_METATYPE(nx::vms::common::AnalyticsPluginResourceList)
