#include "metrics.h"

#include <cmath>

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

ValueManifest::ValueManifest(Label label, Displays display, QString format):
    Label(std::move(label)),
    display(display),
    format(std::move(format))
{
}

ValueGroupManifest::ValueGroupManifest(Label label):
    Label(std::move(label))
{
}

static QString durationString(int64_t duration)
{
    const auto seconds = duration % 60;
    duration /= 60;

    const auto minutes = duration % 60;
    duration /= 60;

    const auto hours = duration % 24;
    duration /= 24;

    return QString("%1%2:%3:%4")
        .arg(duration ? QString("%1 day(s) ").arg(duration) : QString())
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

template<typename Convert>
std::function<Value(const Value&)> numericFormatter(const QString& units, Convert convert)
{
    return
        [units, convert = std::move(convert)](Value value)
        {
            if (!value.isDouble())
                return value;

            const auto data = qRound64(convert(value.toDouble()) * 1000);
            const auto sign = data < 0 ? "-" : "";
            const auto integral = std::abs(data) / 1000;
            const auto fraction = std::abs(data) % 1000;
            if (integral < 1000 && fraction != 0)
                value = QString("%1%2.%3").arg(sign).arg(integral).arg(fraction, 3, 10, QChar('0'));
            else
                value = data / 1000;

            return units.isEmpty() ? value : Value(value.toVariant().toString() + " " + units);
        };
}

std::function<Value(const Value&)> makeFormatter(const QString& targetFormat)
{
    if (targetFormat == "%")
        return numericFormatter(targetFormat, [](double v) { return v * 100; });

    if (targetFormat == "durationS")
        return [](Value v) { return Value(durationString((int64_t) v.toDouble())); };

    if (targetFormat == "Kbps")
        return numericFormatter(targetFormat, [](double v) { return v * 8 / double(1000.0); });

    if (targetFormat == "KPix/s")
        return numericFormatter(targetFormat, [](double v) { return v / double(1000.0); });

    if (targetFormat == "KB"|| targetFormat == "KBps")
        return numericFormatter(targetFormat, [](double v) { return v / double(1024.0); });

    if (targetFormat == "Mbps")
        return numericFormatter(targetFormat, [](double v) { return v * 8 / double(1000.0 * 1000); });

    if (targetFormat == "MPix/s")
        return numericFormatter(targetFormat, [](double v) { return v / double(1000.0 * 1000); });

    if (targetFormat == "MB" || targetFormat == "MBps")
        return numericFormatter(targetFormat, [](double v) { return v / double(1024.0 * 1024); });

    if (targetFormat == "Gbps")
        return numericFormatter(targetFormat, [](double v) { return v * 8 / double(1000.0 * 1000 * 1000); });

    if (targetFormat == "GPix/s")
        return numericFormatter(targetFormat, [](double v) { return v / double(1000.0 * 1000 * 1000); });

    if (targetFormat == "GB" || targetFormat == "GBps")
        return numericFormatter(targetFormat, [](double v) { return v / double(1024.0 * 1024 * 1024); });

    if (targetFormat == "Tbps")
        return numericFormatter(targetFormat, [](double v) { return v * 8 / double(1000.0 * 1000 * 1000 * 1000); });

    if (targetFormat == "TPix/s")
        return numericFormatter(targetFormat, [](double v) { return v / double(1000.0 * 1000 * 1000 * 1000); });

    if (targetFormat == "TB" || targetFormat == "TBps")
        return numericFormatter(targetFormat, [](double v) { return v / double(1024.0 * 1024 * 1024 * 1024); });

    // All unknown formats stay unchanged, but rounded.
    return numericFormatter(targetFormat, [](const double& value) { return value; });
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
