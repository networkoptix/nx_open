#pragma once

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace mediaserver {
namespace metadata {

struct PluginSetting
{
    QString name;
    QString value;
};

#define PluginSetting_Fields (name)(value)

QN_FUSION_DECLARE_FUNCTIONS(PluginSetting, (json))

} // namespace metadata
} // namespace mediaserver
} // namespace nx