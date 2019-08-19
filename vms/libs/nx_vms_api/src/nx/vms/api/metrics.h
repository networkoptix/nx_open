#pragma once

#include <vector>
#include <map>
#include <QtCore/QJsonValue>
#include <QtCore/QJsonObject>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::api::metrics {

using Value = QJsonValue; //< TODO: std::variant<QString, int>.
using Status = QString; //< TODO: enum class { warning, error }.

// -----------------------------------------------------------------------------------------------

struct ParameterRule
{
    std::map<Status, Value> alarms;

    // TODO: Should be optional.
    QString name;
    QString calculate;
    QString insert;
};
#define ParameterRule_Fields (alarms)(name)(calculate)(insert)
QN_FUSION_DECLARE_FUNCTIONS(ParameterRule, (json), NX_VMS_API)

struct ParameterGroupRules: ParameterRule
{
    std::map<QString /*id*/, ParameterGroupRules> group;
};
#define ParameterGroupRules_Fields ParameterRule_Fields (group)
QN_FUSION_DECLARE_FUNCTIONS(ParameterGroupRules, (json), NX_VMS_API)

using ResourceRules
    = std::map<QString /*id*/, ParameterGroupRules>;

using SystemRules
    = std::map<QString /*resourceGroup*/, ResourceRules>;

// -----------------------------------------------------------------------------------------------

struct ParameterManifest
{
    QString id;
    QString name;
    QString unit; //< TODO: Should be optional.
};
#define ParameterManifest_Fields (id)(name)(unit)
QN_FUSION_DECLARE_FUNCTIONS(ParameterManifest, (json), NX_VMS_API)

struct ParameterGroupManifest: ParameterManifest
{
    std::vector<ParameterGroupManifest> group;
};
#define ParameterGroupManifest_Fields ParameterManifest_Fields (group)
QN_FUSION_DECLARE_FUNCTIONS(ParameterGroupManifest, (json), NX_VMS_API)

NX_UTILS_API std::vector<ParameterGroupManifest>::iterator find(
    std::vector<ParameterGroupManifest>& group, const QString& id);

using ResourceManifest
    = std::vector<ParameterGroupManifest>;

using SystemManifest
    = std::map<QString /*resourceGroup*/, ResourceManifest>;

NX_UTILS_API ParameterGroupManifest makeParameterManifest(
    QString id, QString name = {}, QString unit = {});

NX_UTILS_API ParameterGroupManifest makeParameterGroupManifest(
    QString id, QString name = {}, std::vector<ParameterGroupManifest> group = {});

// -----------------------------------------------------------------------------------------------

struct ParameterValue
{
    Value value;
    Status status;
};
#define ParameterValue_Fields (value)(status)
QN_FUSION_DECLARE_FUNCTIONS(ParameterValue, (json), NX_VMS_API)

struct ParameterGroupValues: ParameterValue
{
    std::map<QString /*id*/, ParameterGroupValues> group;
};
#define ParameterGroupValues_Fields ParameterValue_Fields (group)
QN_FUSION_DECLARE_FUNCTIONS(ParameterGroupValues, (json), NX_VMS_API)

NX_UTILS_API ParameterGroupValues makeParameterValue(
    Value value, Status status = {});

NX_UTILS_API ParameterGroupValues makeParameterGroupValue(
    std::map<QString, ParameterGroupValues> group);

struct ResourceValues
{
    QString name;
    QString parent;
    std::map<QString /*id*/, ParameterGroupValues> values;
};
#define ResourceValues_Fields (name)(parent)(values)
QN_FUSION_DECLARE_FUNCTIONS(ResourceValues, (json), NX_VMS_API)

using ResourceGroupValues =
    std::map<QString /*resourceId*/, ResourceValues>;

using SystemValues
    = std::map<QString /*resourceGroup*/, ResourceGroupValues>;

NX_UTILS_API void merge(SystemValues* destination, SystemValues* source);
NX_UTILS_API SystemValues merge(std::vector<SystemValues> valuesList);

} // namespace nx::vms::api::metrics
