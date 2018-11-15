#pragma once

#include <QtCore/QString>
#include <QtCore/QMap>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::api::analytics {

struct NX_VMS_API TranslatableString
{
    QString value;
    QMap<QString, QString> localization;

    QString text(const QString& locale) const;
};
#define TranslatableString_Fields (value)(localization)

QN_FUSION_DECLARE_FUNCTIONS(TranslatableString, (json), NX_VMS_API)

} // namespace nx::vms::api::analytics
