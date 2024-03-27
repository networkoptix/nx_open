// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QList>
#include <QtCore/QStringList>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/instrument.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/analytics/manifest_error.h>

namespace nx::vms::api::analytics {

/**%apidoc
 * The data structure that is given by each Analytics Integration after the Integration has been
 * created.
 * <br/>
 * See the description of the fields in `src/nx/sdk/analytics/manifests.md` in Metadata SDK.
 */
struct NX_VMS_API PluginManifest
{
    Q_GADGET

public: //< Required for Qt MOC run.
    QString id;
    QString name;
    QString description;
    QString version;

    /**%apidoc[opt] */
    QString vendor;

    /**%apidoc[opt] */
    QJsonObject engineSettingsModel;

    /**%apidoc[opt] */
    bool isLicenseRequired = false;

    bool operator==(const PluginManifest& other) const = default;
};

#define nx_vms_api_analytics_PluginManifest_Fields \
    (id)(name)(description)(version)(vendor)(engineSettingsModel)(isLicenseRequired)

QN_FUSION_DECLARE_FUNCTIONS(PluginManifest, (json), NX_VMS_API)

NX_REFLECTION_INSTRUMENT(PluginManifest, nx_vms_api_analytics_PluginManifest_Fields);

NX_VMS_API std::vector<ManifestError> validate(const PluginManifest& pluginManifest);

using IntegrationManifest = PluginManifest;

} // namespace nx::vms::api::analytics
