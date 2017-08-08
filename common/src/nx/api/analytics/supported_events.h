#pragma once

#include <QtCore/QList>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace api {

/**
* Subset of events, applicable to the given camera.
*/
struct AnalyticsSupportedEvents
{
    QnUuid driverId;
    QList<QnUuid> eventTypes;
};
#define AnalyticsSupportedEvents_Fields (driverId)(eventTypes)

QN_FUSION_DECLARE_FUNCTIONS(AnalyticsSupportedEvents, (json)(metatype))

} // namespace api
} // namespace nx
