#pragma once

#include <QtCore/QList>
#include <QtCore/QStringList>
#include <QtCore/QJsonObject>

#include <nx/vms/api/analytics/manifest_items.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx::vms::api::analytics {

struct NX_VMS_API PluginManifest
{
    Q_GADGET

public: //< Required for Qt MOC run.
    QString id;
    QString name;
    QJsonObject engineSettingsModel;
};

#define nx_vms_api_analytics_PluginManifest_Fields \
    (id)(name)(engineSettingsModel)

QN_FUSION_DECLARE_FUNCTIONS(PluginManifest, (json), NX_VMS_API)

} // namespace nx::vms::api::analytics
