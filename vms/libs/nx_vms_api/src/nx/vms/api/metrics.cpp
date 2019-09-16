#include "metrics.h"

#include <nx/fusion/model_functions.h>

namespace nx::vms::api::metrics {

void PrintTo(const Value& v, ::std::ostream* s)
{
    *s << QJson::serialized(v).toStdString();
}

NX_VMS_API void merge(Value* destination, Value* source)
{
    *destination = *source;
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
