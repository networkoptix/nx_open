// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/text/human_readable.h>

namespace nx::vms::client::core {

class DurationFormatter: public QObject
{
    Q_OBJECT
    Q_FLAGS(nx::vms::text::HumanReadable::TimeSpanFormat)
    Q_ENUMS(nx::vms::text::HumanReadable::SuffixFormat)

public:
    explicit DurationFormatter(QObject* parent = nullptr);

    Q_INVOKABLE QString toString(qint64 durationMs,
        text::HumanReadable::TimeSpanFormat format = text::HumanReadable::DaysAndTime,
        text::HumanReadable::SuffixFormat suffixFormat = text::HumanReadable::SuffixFormat::Full,
        const QString& separator = QLatin1String(", "));

    static void registerQmlType();
};

} // namespace nx::vms::client::core
