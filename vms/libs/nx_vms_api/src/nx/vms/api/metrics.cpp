#include "metrics.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::metrics {

void PrintTo(const Value& v, ::std::ostream* s)
{
    *s << QJson::serialized(v).toStdString();
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ValueManifest)(ValueGroupManifest)(AlarmRule)(ValueRule)(ValueGroupRules)(Alarm),
    (json), _Fields, (optional, true))

static QString nameOrCapitalizedId(QString name, const QString& id)
{
    if (!name.isEmpty() || id.isEmpty())
        return name;

    return id.left(1).toUpper() + id.mid(1);
}

Label::Label(QString id, QString name):
    id(std::move(id)),
    name(nameOrCapitalizedId(std::move(name), this->id))
{
}

ValueManifest::ValueManifest(Label label, Displays display, QString unit):
    Label(std::move(label)),
    display(display),
    unit(std::move(unit))
{
}

ValueGroupManifest::ValueGroupManifest(Label label):
    Label(std::move(label))
{
}

void merge(SystemValues* destination, SystemValues* source)
{
    for (auto& [group, sourceResources]: *source)
    {
        auto& destinationResources = (*destination)[group];
        for (auto& [resourceId, values]: sourceResources)
            destinationResources[resourceId] = std::move(values);
    }
}

void merge(Alarms* destination, Alarms* source)
{
    destination->insert(destination->end(), source->begin(), source->end());
}

} // namespace nx::vms::api::metrics

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::metrics, AlarmLevel,
    (nx::vms::api::metrics::AlarmLevel::warning, "warning")
    (nx::vms::api::metrics::AlarmLevel::danger, "danger")
)

#define METRICS_DISPLAY_NAMES \
    (nx::vms::api::metrics::Display::none, "") \
    (nx::vms::api::metrics::Display::table, "table") \
    (nx::vms::api::metrics::Display::panel, "panel")
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::metrics, Display, METRICS_DISPLAY_NAMES)
QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(nx::vms::api::metrics, Displays, METRICS_DISPLAY_NAMES)
