// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtQml/QtQml>

#include <nx/vms/text/human_readable.h>

#include "duration_formatter.h"

namespace nx::vms::client::core {

using namespace std::chrono;
using nx::vms::text::HumanReadable;

DurationFormatter::DurationFormatter(QObject* parent): QObject(parent)
{
}

QString DurationFormatter::toString(qint64 durationMs,
    HumanReadable::TimeSpanFormat format,
    HumanReadable::SuffixFormat suffixFormat,
    const QString& separator)
{
    return HumanReadable::timeSpan(milliseconds(durationMs), format, suffixFormat, separator);
}

void DurationFormatter::registerQmlType()
{
    qmlRegisterSingletonType<DurationFormatter>("nx.vms.client.core", 1, 0, "Duration",
        [](QQmlEngine*, QJSEngine*) { return new DurationFormatter(); });
}

} // namespace nx::vms::client::core
