// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QLocale>
#include <QtCore/QObject>

#include <nx/vms/client/core/timeline/data/timeline_zoom_level.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::core {
namespace timeline {

class NX_VMS_CLIENT_CORE_API LabelFormatter: public QObject
{
    Q_OBJECT
    Q_CLASSINFO("RegisterEnumClassesUnscoped", "false")
    Q_PROPERTY(QLocale locale READ locale WRITE setLocale NOTIFY localeChanged)

public:
    explicit LabelFormatter(QObject* parent = nullptr);
    virtual ~LabelFormatter() override;

    /** Format time scale tick label. */
    Q_INVOKABLE QString formatLabel(qint64 timeMs, const QTimeZone& timeZone,
        TimelineZoomLevel::LevelType level) const;

    /** Format time marker inside current time window. */
    Q_INVOKABLE QString formatMarker(
        qint64 timeMs, const QTimeZone& timeZone, qreal millisecondsPerPixel) const;

    /** Format time marker outside current time window. */
    Q_INVOKABLE QString formatOutsideMarker(
        qint64 timeMs, const QTimeZone& timeZone, TimelineZoomLevel::LevelType level) const;

    /** Format time marker outside current time window. */
    Q_INVOKABLE QString formatHeader(qint64 startTimeMs, qint64 endTimeMs,
        const QTimeZone& timeZone) const;

    /** Format timestamp for external uses (e.g. in object details sheet). */
    Q_INVOKABLE QString formatTimestamp(qint64 timestampMs, const QTimeZone& timeZone) const;

    QLocale locale() const;
    void setLocale(QLocale value);

    static void registerQmlType();

signals:
    void localeChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace timeline
} // namespace nx::vms::client::core
