#pragma once

#include <vector>
#include <map>
#include <QtCore/QJsonValue>
#include <QtCore/QJsonObject>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::api::metrics {

//< TODO: std::variant<QString, int>.
struct NX_VMS_API Value: QJsonValue { using QJsonValue::QJsonValue; };
NX_VMS_API void PrintTo(const Value& v, ::std::ostream* s);

using ValueGroup
    = std::map<QString /*parameterId*/, Value>;

using ResourceValues =
    std::map<QString /*groupId*/, ValueGroup>;

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

    Label(QString id = {}, QString name = {});
};

enum class Display
{
    none = 0,
    table = (1 << 0),
    panel = (1 << 1),
    both = table|panel,
};
Q_DECLARE_FLAGS(Displays, Display)

struct NX_VMS_API ValueManifest: Label
{
    Displays display;
    QString unit;

    ValueManifest(Label label = {}, Displays display = Display::none, QString unit = {});
};
#define ValueManifest_Fields (id)(name)(unit)(display)
QN_FUSION_DECLARE_FUNCTIONS(ValueManifest, (json), NX_VMS_API)

struct NX_VMS_API ValueGroupManifest: Label
{
    std::vector<ValueManifest> values;

    ValueGroupManifest(Label label = {});
};
#define ValueGroupManifest_Fields (id)(name)(values)
QN_FUSION_DECLARE_FUNCTIONS(ValueGroupManifest, (json), NX_VMS_API)

using ResourceManifest
    = std::vector<ValueGroupManifest>;

using SystemManifest
    = std::map<QString /*resourceTypeGroup*/, ResourceManifest>;

// -----------------------------------------------------------------------------------------------

enum class AlarmLevel { warning, danger };

struct NX_VMS_API AlarmRule
{
    AlarmLevel level;
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

struct ValueGroupRules
{
    QString name;
    std::map<QString /*id*/, ValueRule> values;
};
#define ValueGroupRules_Fields (name)(values)
QN_FUSION_DECLARE_FUNCTIONS(ValueGroupRules, (json), NX_VMS_API)

using ResourceRules
    = std::map<QString /*id*/, ValueGroupRules>;

using SystemRules
    = std::map<QString /*resourceTypeGroup*/, ResourceRules>;

// -----------------------------------------------------------------------------------------------

struct NX_VMS_API Alarm
{
    QString resource;
    QString parameter;
    AlarmLevel level;
    QString text;
};
#define Alarm_Fields (resource)(parameter)(level)(text)
QN_FUSION_DECLARE_FUNCTIONS(Alarm, (json), NX_VMS_API)

using Alarms = std::vector<Alarm>;

NX_UTILS_API void merge(Alarms* destination, Alarms* source);

} // namespace nx::vms::api::metrics

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::metrics::Display, (lexical), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::metrics::Displays, (lexical), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::metrics::AlarmLevel, (lexical), NX_VMS_API)
