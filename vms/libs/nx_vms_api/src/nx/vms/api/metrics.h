// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <functional>
#include <map>
#include <vector>

#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/serialization/flags.h>

namespace nx::vms::api::metrics {

class NX_VMS_API Value: public QJsonValue //< TODO: std::variant<QString, int>.
{
public:
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
    /**%apidoc Parameter id. */
    QString id;

    /**%apidoc Parameter name. */
    QString name;

    Label(QString id = {}, QString name = {});
};

enum class Display
{
    /**%apidoc[unused] */
    none = 0,
    table = (1 << 0),
    panel = (1 << 1),

    /**%apidoc[unused] */
    both = table|panel,
};
Q_DECLARE_FLAGS(Displays, Display)

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(Display*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<Display>;
    return visitor(
        Item{Display::none, ""},
        Item{Display::table, "table"},
        Item{Display::panel, "panel"}
    );
}

struct NX_VMS_API ValueManifest: Label
{
    /**%apidoc Parameter description. */
    QString description;

    /**%apidoc Display type. */
    Displays display;

    /**%apidoc Parameter format or units. */
    QString format;

    ValueManifest(QString id = {}, QString name = {});
};
#define ValueManifest_Fields (id)(name)(description)(format)(display)
QN_FUSION_DECLARE_FUNCTIONS(ValueManifest, (json), NX_VMS_API)

/**%apidoc
 * %param id Parameter group id.
 * %param name Parameter group name.
 */
struct NX_VMS_API ValueGroupManifest: Label
{
    /**%apidoc Parameter group manifest. */
    std::vector<ValueManifest> values;

    using Label::Label;
};
#define ValueGroupManifest_Fields (id)(name)(values)
QN_FUSION_DECLARE_FUNCTIONS(ValueGroupManifest, (json), NX_VMS_API)

/**%apidoc
 * %param id Resource group id.
 * %param name Resource group name.
 */
struct NX_VMS_API ResourceManifest: Label
{
    /**%apidoc Resource label. */
    QString resource;

    /**%apidoc Resource group manifest. */
    std::vector<ValueGroupManifest> values;

    using Label::Label;
};
#define ResourceManifest_Fields (id)(name)(resource)(values)
QN_FUSION_DECLARE_FUNCTIONS(ResourceManifest, (json), NX_VMS_API)

using SystemManifest
    = std::vector<ResourceManifest>;

// -----------------------------------------------------------------------------------------------

enum class AlarmLevel
{
    /**%apidoc[unused] */
    none,
    warning,
    error,
};

template<typename Visitor>
constexpr auto nxReflectVisitAllEnumItems(AlarmLevel*, Visitor&& visitor)
{
    using Item = nx::reflect::enumeration::Item<AlarmLevel>;
    return visitor(
        Item{AlarmLevel::none, ""},
        Item{AlarmLevel::warning, "warning"},
        Item{AlarmLevel::error, "error"}
    );
}

struct NX_VMS_API AlarmRule
{
    AlarmLevel level = AlarmLevel::none;
    QString condition;
    QString text; //< TODO: Optional.
};
#define AlarmRule_Fields (level)(condition)(text)
QN_FUSION_DECLARE_FUNCTIONS(AlarmRule, (json), NX_VMS_API)

struct NX_VMS_API ValueRule
{
    QString name;
    QString description;
    bool isOptional = false;
    Displays display;
    QString format;
    QString calculate;
    QString insert;
    std::vector<AlarmRule> alarms;
};
#define ValueRule_Fields (name)(description)(isOptional)(display)(format)(calculate)(insert)(alarms)
QN_FUSION_DECLARE_FUNCTIONS(ValueRule, (json), NX_VMS_API)

struct NX_VMS_API ValueGroupRules
{
    QString name;
    std::map<QString, ValueRule> values;
};
#define ValueGroupRules_Fields (name)(values)
QN_FUSION_DECLARE_FUNCTIONS(ValueGroupRules, (json), NX_VMS_API)

struct NX_VMS_API ResourceRules
{
    QString name;
    QString resource;
    std::map<QString, ValueGroupRules> values;
};
#define ResourceRules_Fields (name)(resource)(values)
QN_FUSION_DECLARE_FUNCTIONS(ResourceRules, (json), NX_VMS_API)

using SystemRules
    = std::map<QString /*resourceGroupId*/, ResourceRules>;

// -----------------------------------------------------------------------------------------------

struct NX_VMS_API Alarm
{
    AlarmLevel level = AlarmLevel::none;
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

void NX_VMS_API apply(const ValueRule& rule, ValueManifest* manifest);

NX_VMS_API std::function<Value(const Value&)> makeFormatter(const QString& targetFormat);

} // namespace nx::vms::api::metrics
