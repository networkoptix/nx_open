// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ValueManifest, (json), ValueManifest_Fields, (optional, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ValueGroupManifest, (json), ValueGroupManifest_Fields, (optional, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ResourceManifest, (json), ResourceManifest_Fields, (optional, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(AlarmRule, (json), AlarmRule_Fields, (optional, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ResourceRules, (json), ResourceRules_Fields, (optional, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ValueRule, (json), ValueRule_Fields, (optional, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(ValueGroupRules, (json), ValueGroupRules_Fields, (optional, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Alarm, (json), Alarm_Fields, (optional, true))

static QString nameOrCapitalizedId(QString name, const QString& id)
{
    if (!name.isEmpty() || id.isEmpty() || id == "_")
        return name;

    return id.left(1).toUpper() + id.mid(1);
}

Label::Label(QString id, QString name):
    id(std::move(id)),
    name(nameOrCapitalizedId(std::move(name), this->id))
{
}

ValueManifest::ValueManifest(QString id, QString name):
    Label(std::move(id), std::move(name))
{
}

void apply(const ValueRule& rule, ValueManifest* manifest)
{
    if (!rule.name.isEmpty())
        manifest->name = rule.name;

    manifest->description = rule.description;
    manifest->display = rule.display;
    manifest->format = rule.format;
}

static Value durationString(const Value& value, bool hoursOnly)
{
    if (!value.isDouble())
        return value;

    int64_t duration(value.toDouble());

    const auto seconds = duration % 60;
    duration /= 60;

    const auto minutes = duration % 60;
    duration /= 60;

    const auto hours = duration % 24;
    duration /= 24;

    const auto daysString = duration ? QString("%1d ").arg(duration) : QString();
    if (hoursOnly)
        return QString("%1%2h").arg(daysString).arg(hours);

    return QString("%1%2:%3:%4")
        .arg(daysString)
        .arg(hours, 2, 10, QChar('0'))
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

template<typename Convert>
std::function<Value(const Value&)> numericFormatter(
    const QString& units, Convert convert, bool addSpace = true)
{
    return
        [suffix = units.isEmpty() ? QString() : (addSpace ? (" " + units) : units),
            convert = std::move(convert)](const Value& value)
        {
            if (!value.isDouble())
                return value;

            // If the absolute value is above the threshold, fraction should be omitted. Otherwise
            // fixed precision without trailing zeros should be applied.
            static const double kFractionThreshold = 10;
            static const int kFractionDigits = 2;

            const auto number = convert(value.toDouble());
            QString string;
            if (std::abs(number) >= kFractionThreshold)
            {
                string = QString::number(number, 'f', 0);
            }
            else
            {
                string = QString::number(number, 'f', kFractionDigits);
                while (string.endsWith('0')) string.chop(1);
                if (string.endsWith('.')) string.chop(1);
            }

            return Value(string + suffix);
        };
}

std::function<Value(const Value&)> makeFormatter(const QString& targetFormat)
{
    if (targetFormat == "text" || targetFormat == "shortText" || targetFormat == "longText")
        return [](const Value& v) { return v.toVariant().toString(); }; // String without units.

    if (targetFormat == "number")
        return numericFormatter("", [](double v) { return v; }); // Rounded number without units.

    if (targetFormat == "%")
        return numericFormatter(targetFormat, [](double v) { return v * 100; }, /*addSpace*/ false);

    if (targetFormat == "duration")
        return [](const Value& v) { return durationString(v, /*hoursOnly*/ false); };

    if (targetFormat == "durationDh")
        return [](const Value& v) { return durationString(v, /*hoursOnly*/ true); };

    if (targetFormat == "bit" || targetFormat == "bit/s")
        return numericFormatter(targetFormat, [](double v) { return v * 8; });

    if (targetFormat == "Kbit" || targetFormat == "Kbit/s")
        return numericFormatter(targetFormat, [](double v) { return v * 8 / double(1000.0); });

    if (targetFormat == "KPix/s")
        return numericFormatter(targetFormat, [](double v) { return v / double(1000.0); });

    if (targetFormat == "KB"|| targetFormat == "KB/s")
        return numericFormatter(targetFormat, [](double v) { return v / double(1024.0); });

    if (targetFormat == "Mbit" || targetFormat == "Mbit/s")
        return numericFormatter(targetFormat, [](double v) { return v * 8 / double(1000.0 * 1000); });

    if (targetFormat == "MPix/s")
        return numericFormatter(targetFormat, [](double v) { return v / double(1000.0 * 1000); });

    if (targetFormat == "MB" || targetFormat == "MB/s")
        return numericFormatter(targetFormat, [](double v) { return v / double(1024.0 * 1024); });

    if (targetFormat == "Gbit" || targetFormat == "Gbit/s")
        return numericFormatter(targetFormat, [](double v) { return v * 8 / double(1000.0 * 1000 * 1000); });

    if (targetFormat == "GPix/s")
        return numericFormatter(targetFormat, [](double v) { return v / double(1000.0 * 1000 * 1000); });

    if (targetFormat == "GB" || targetFormat == "GB/s")
        return numericFormatter(targetFormat, [](double v) { return v / double(1024.0 * 1024 * 1024); });

    if (targetFormat == "Tbit" || targetFormat == "Tbit/s")
        return numericFormatter(targetFormat, [](double v) { return v * 8 / double(1000.0 * 1000 * 1000 * 1000); });

    if (targetFormat == "TPix/s")
        return numericFormatter(targetFormat, [](double v) { return v / double(1000.0 * 1000 * 1000 * 1000); });

    if (targetFormat == "TB" || targetFormat == "TB/s")
        return numericFormatter(targetFormat, [](double v) { return v / double(1024.0 * 1024 * 1024 * 1024); });

    // All unknown formats stay unchanged, but rounded.
    return numericFormatter(targetFormat, [](const double& value) { return value; });
}

} // namespace nx::vms::api::metrics
