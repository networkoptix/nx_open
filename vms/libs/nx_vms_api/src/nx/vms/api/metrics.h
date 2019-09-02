#pragma once

#include <vector>
#include <map>
#include <QtCore/QJsonValue>
#include <QtCore/QJsonObject>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::api::metrics {

using Value = QJsonValue; //< TODO: std::variant<QString, int>.

using ValueGroup
    = std::map<QString /*parameterId*/, Value>;

struct NX_VMS_API ResourceValues
{
    QString name;
    QString parent;
    std::map<QString /*groupId*/, ValueGroup> values;
};
#define ResourceValues_Fields (name)(parent)(values)
QN_FUSION_DECLARE_FUNCTIONS(ResourceValues, (json), NX_VMS_API)

using ResourceGroupValues =
    std::map<QString /*resourceId*/, ResourceValues>;

using SystemValues
    = std::map<QString /*resourceGroup*/, ResourceGroupValues>;

NX_VMS_API void merge(SystemValues* destination, SystemValues* source);

// -----------------------------------------------------------------------------------------------

struct NX_VMS_API Label
{
    QString id;
    QString name;
};

struct NX_VMS_API ValueManifest: Label
{
    QString display; //< TODO: Optional.
    QString unit; //< TODO: Optional.
};
#define ValueManifest_Fields (id)(name)(unit)(display)
QN_FUSION_DECLARE_FUNCTIONS(ValueManifest, (json), NX_VMS_API)

struct NX_VMS_API ValueGroupManifest: Label
{
    ValueGroupManifest(Label label = {}): Label(std::move(label)) {}

    std::vector<ValueManifest> values;
};
#define ValueGroupManifest_Fields (id)(name)(values)
QN_FUSION_DECLARE_FUNCTIONS(ValueGroupManifest, (json), NX_VMS_API)

using ResourceManifest
    = std::vector<ValueGroupManifest>;

using SystemManifest
    = std::map<QString /*resourceTypeGroup*/, ResourceManifest>;

// -----------------------------------------------------------------------------------------------

struct NX_VMS_API AlarmRule
{
    QString level;
    QString condition;
    QString text; //< TODO: Optional.
};
#define AlarmRule_Fields (level)(condition)(text)
QN_FUSION_DECLARE_FUNCTIONS(AlarmRule, (json), NX_VMS_API)

struct NX_VMS_API ValueRule: ValueManifest
{
    QString calculate;
    QString insert;
    std::vector<AlarmRule> alarms;
};
#define ValueRule_Fields ValueManifest_Fields(calculate)(insert)(alarms)
QN_FUSION_DECLARE_FUNCTIONS(ValueRule, (json), NX_VMS_API)

using ValueGroupRules
    = std::map<QString /*id*/, ValueRule>;

using ResourceRules
    = std::map<QString /*id*/, ValueGroupRules>;

using SystemRules
    = std::map<QString /*resourceTypeGroup*/, ResourceRules>;

// -----------------------------------------------------------------------------------------------

struct NX_VMS_API Alarm
{
    QString resource;
    QString parameter;
    QString level;
    QString text;
};
#define Alarm_Fields (resource)(parameter)(level)(text)
QN_FUSION_DECLARE_FUNCTIONS(Alarm, (json), NX_VMS_API)

using Alarms = std::vector<Alarm>;

NX_UTILS_API void merge(Alarms* destination, Alarms* source);

} // namespace nx::vms::api::metrics
