#pragma once

#include <set>
#include <memory>
#include <nx/fusion/model_functions_fwd.h>

class QnAbstractDataReceptor;

namespace nx::vms::server::analytics {

using MetadataSinkPtr = std::shared_ptr<QnAbstractDataReceptor>;
using MetadataSinkSet = std::set<MetadataSinkPtr>;

enum class SettingsOrigin
{
    server,
    plugin,
};

QString toString(SettingsOrigin settingsOrigin);

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(SettingsOrigin);

} // namespace nx::vms::server::analytics

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::server::analytics::SettingsOrigin, (lexical))
