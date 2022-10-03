// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
class NX_VMS_COMMON_API AnalyticsPluginResource: public QnResource
{
    Q_OBJECT
    using base_type = QnResource;

public:
    static QString kPluginManifestProperty;

    AnalyticsPluginResource();
    virtual ~AnalyticsPluginResource() override = default;

    vms::api::analytics::PluginManifest manifest() const;
    void setManifest(const vms::api::analytics::PluginManifest& manifest);

    AnalyticsEngineResourceList engines() const;
    bool hasDefaultEngine() const;

    virtual QString idForToStringFromPtr() const override;
};

} // namespace nx::vms::common

Q_DECLARE_METATYPE(nx::vms::common::AnalyticsPluginResourcePtr)
Q_DECLARE_METATYPE(nx::vms::common::AnalyticsPluginResourceList)
