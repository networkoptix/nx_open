#pragma once

#include <QtCore/QList>
#include <QtCore/QStringList>

#include <nx/api/common/translatable_string.h>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace api {

struct AnalyticsEventType
{
    QnUuid eventId;
    TranslatableString eventName;
};
#define AnalyticsEventType_Fields (eventId)(eventName)

} // namespace api
} // namespace nx

QN_FUSION_DECLARE_FUNCTIONS(nx::api::AnalyticsEventType, (json)(metatype))
