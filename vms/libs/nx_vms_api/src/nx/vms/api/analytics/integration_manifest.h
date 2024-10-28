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
 * See the description of the fields in `src/nx/sdk/analytics/manifests.md` in the SDK.
 */
struct NX_VMS_API IntegrationManifest
{
    Q_GADGET

public: //< Required for Qt MOC run.
    QString id;
    QString name;
    QString description;
    QString version;

    /**%apidoc[opt]
     * TODO: Investigate why it's [opt], contrary to manifests.md saying it's mandatory.
     */
    QString vendor;

    /**%apidoc[opt] */
    QJsonObject engineSettingsModel;

    /**%apidoc[opt] */
    bool isLicenseRequired = false;

    bool operator==(const IntegrationManifest& other) const = default;
};

#define nx_vms_api_analytics_IntegrationManifest_Fields \
    (id)(name)(description)(version)(vendor)(engineSettingsModel)(isLicenseRequired)

QN_FUSION_DECLARE_FUNCTIONS(IntegrationManifest, (json), NX_VMS_API)

NX_REFLECTION_INSTRUMENT(IntegrationManifest, nx_vms_api_analytics_IntegrationManifest_Fields);

NX_VMS_API std::vector<ManifestError> validate(const IntegrationManifest& integrationManifest);

} // namespace nx::vms::api::analytics
