#pragma once

#include <QtCore>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/fusion/fusion/fusion_fwd.h>

namespace nx {
namespace api {

struct AvailableUpdatesInfo
{
    QString version;

    AvailableUpdatesInfo(const QString& version): version(version) {}
    AvailableUpdatesInfo() = default;
};
#define AvailableUpdatesInfo_Fields (version)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((AvailableUpdatesInfo), (json))

} // namespace api
} // namespace nx
