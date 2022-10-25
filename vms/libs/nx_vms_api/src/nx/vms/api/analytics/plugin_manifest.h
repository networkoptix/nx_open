// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QStringList>
#include <QtCore/QJsonObject>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/analytics/manifest_error.h>
#include <nx/vms/api/analytics/manifest_error.h>

namespace nx::vms::api::analytics {

struct NX_VMS_API PluginManifest
{
    Q_GADGET

public: //< Required for Qt MOC run.
    QString id;
    QString name;
    QString description;
    QString version;
    QString vendor;
    QJsonObject engineSettingsModel;
};

#define nx_vms_api_analytics_PluginManifest_Fields \
    (id)(name)(description)(version)(vendor)(engineSettingsModel)

QN_FUSION_DECLARE_FUNCTIONS(PluginManifest, (json), NX_VMS_API)

NX_REFLECTION_INSTRUMENT(PluginManifest, nx_vms_api_analytics_PluginManifest_Fields);

NX_VMS_API std::vector<ManifestError> validate(const PluginManifest& pluginManifest);

using IntegrationManifest = PluginManifest;

} // namespace nx::vms::api::analytics
