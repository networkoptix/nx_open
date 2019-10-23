#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <vector>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>

#include <nx/fusion/model_functions_fwd.h>

namespace nx::vms::api::metrics {

struct NX_VMS_API Value: QJsonValue //< TODO: std::variant<QString, int>.
{
    using QJsonValue::QJsonValue;
    Value(std::chrono::milliseconds duration): QJsonValue(double(duration.count()) / 1000) {}
};

NX_VMS_API void PrintTo(const Value& v, ::std::ostream* s);
NX_VMS_API void merge(Value* destination, Value* source);

using ValueGroup
    = std::map<QString /*parameterId*/, Value>;

using ResourceValues =
    std::map<QString /*groupId*/, ValueGroup>;

using ResourceGroupValues =
    std::map<QString /*resourceId*/, ResourceValues>;

using SystemValues
    = std::map<QString /*resourceGroup*/, ResourceGroupValues>;

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
    QString description;
    Displays display;
    QString format;

    ValueManifest(Label label = {}, Displays display = Display::none, QString format = {});
};
#define ValueManifest_Fields (id)(name)(description)(format)(display)
QN_FUSION_DECLARE_FUNCTIONS(ValueManifest, (json), NX_VMS_API)

struct NX_VMS_API ValueGroupManifest: Label
{
    std::vector<ValueManifest> values;

    using Label::Label;
};
#define ValueGroupManifest_Fields (id)(name)(values)
QN_FUSION_DECLARE_FUNCTIONS(ValueGroupManifest, (json), NX_VMS_API)

struct NX_VMS_API ResourceManifest: Label
{
    QString resource;
    std::vector<ValueGroupManifest> values;

    using Label::Label;
};
#define ResourceManifest_Fields (id)(name)(resource)(values)
QN_FUSION_DECLARE_FUNCTIONS(ResourceManifest, (json), NX_VMS_API)

using SystemManifest
    = std::vector<ResourceManifest>;

// -----------------------------------------------------------------------------------------------

enum class AlarmLevel { none, warning, error };

struct NX_VMS_API AlarmRule
{
    AlarmLevel level;
    QString condition;
    QString text; //< TODO: Optional.
};
#define AlarmRule_Fields (level)(condition)(text)
QN_FUSION_DECLARE_FUNCTIONS(AlarmRule, (json), NX_VMS_API)

struct NX_VMS_API ValueRule
{
    QString name;
    QString description;
    Displays display;
    QString format;
    QString calculate;
    QString insert;
    std::vector<AlarmRule> alarms;
};
#define ValueRule_Fields (name)(description)(display)(format)(calculate)(insert)(alarms)
QN_FUSION_DECLARE_FUNCTIONS(ValueRule, (json), NX_VMS_API)

struct NX_VMS_API ValueGroupRules
{
    QString name;
    std::map<QString /*id*/, ValueRule> values;
};
#define ValueGroupRules_Fields (name)(values)
QN_FUSION_DECLARE_FUNCTIONS(ValueGroupRules, (json), NX_VMS_API)

struct NX_VMS_API ResourceRules
{
    QString name;
    QString resource;
    std::map<QString /*id*/, ValueGroupRules> values;
};
#define ResourceRules_Fields (name)(resource)(values)
QN_FUSION_DECLARE_FUNCTIONS(ResourceRules, (json), NX_VMS_API)

using SystemRules
    = std::map<QString /*resourceGroupId*/, ResourceRules>;

// -----------------------------------------------------------------------------------------------

struct NX_VMS_API Alarm
{
    AlarmLevel level;
    QString text;
};
#define Alarm_Fields (level)(text)
QN_FUSION_DECLARE_FUNCTIONS(Alarm, (json), NX_VMS_API)

using ValueAlarms
    = std::vector<Alarm>;

using ValueGroupAlarms
    = std::map<QString /*id*/, ValueAlarms>;

using ResourceAlarms
    = std::map<QString /*groupId*/, ValueGroupAlarms>;

using ResourceGroupAlarms =
    std::map<QString /*resourceId*/, ResourceAlarms>;

using SystemAlarms
    = std::map<QString /*resourceGroupId*/, ResourceGroupAlarms>;

// -----------------------------------------------------------------------------------------------

template<typename Value>
void merge(std::map<QString, Value>* destination, std::map<QString, Value>* source)
{
    for (auto& [id, value]: *source)
        merge(&(*destination)[id], &value);
}

template<typename Value>
void merge(std::vector<Value>* destination, std::vector<Value>* source)
{
    destination->insert(destination->end(), source->begin(), source->end());
}

NX_VMS_API std::function<Value(const Value&)> makeFormatter(const QString& targetFormat);

} // namespace nx::vms::api::metrics

QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::metrics::Display, (lexical), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::metrics::Displays, (lexical), NX_VMS_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::vms::api::metrics::AlarmLevel, (lexical), NX_VMS_API)
