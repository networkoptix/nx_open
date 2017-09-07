#pragma once

#include <QtCore/QString>
#include <QtCore/QMap>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace api {

struct TranslatableString
{
    QString value;
    QMap<QString, QString> localization;

    QString text(const QString& locale) const;
};
#define TranslatableString_Fields (value)(localization)

QN_FUSION_DECLARE_FUNCTIONS(TranslatableString, (json))

} // namespace api
} // namespace nx
