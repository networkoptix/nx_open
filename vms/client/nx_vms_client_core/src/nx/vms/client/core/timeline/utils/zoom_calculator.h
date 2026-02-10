// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QTimeZone>

#include <nx/vms/client/core/timeline/data/timeline_zoom_level.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::core {
namespace timeline {

class NX_VMS_CLIENT_CORE_API ZoomCalculator: public QObject
{
    Q_OBJECT
    Q_PROPERTY(qint64 startTimeMs READ startTimeMs WRITE setStartTimeMs NOTIFY startTimeChanged)
    Q_PROPERTY(qint64 durationMs READ durationMs WRITE setDurationMs NOTIFY durationChanged)
    Q_PROPERTY(qreal minimumTickSpacingMs READ minimumTickSpacingMs
        WRITE setMinimumTickSpacingMs NOTIFY minimumTickSpacingChanged)

    Q_PROPERTY(TimelineZoomLevel::LevelType majorTicksLevel READ majorTicksLevel
        NOTIFY majorTicksLevelChanged)

    Q_PROPERTY(QList<qint64> majorTicks READ majorTicks NOTIFY majorTicksChanged)
    Q_PROPERTY(QList<qint64> minorTicks READ minorTicks NOTIFY minorTicksChanged)

    Q_PROPERTY(QTimeZone timeZone READ timeZone WRITE setTimeZone NOTIFY timeZoneChanged)

public:
    explicit ZoomCalculator(QObject* parent = nullptr);
    virtual ~ZoomCalculator() override;

    qint64 startTimeMs() const;
    void setStartTimeMs(qint64 value);

    qint64 durationMs() const;
    void setDurationMs(qint64 value);

    qreal minimumTickSpacingMs() const;
    void setMinimumTickSpacingMs(qreal value);

    TimelineZoomLevel::LevelType majorTicksLevel() const;
    QList<qint64> majorTicks() const;

    QList<qint64> minorTicks() const;

    QTimeZone timeZone() const;
    void setTimeZone(const QTimeZone& value);

    static void registerQmlType();

signals:
    void startTimeChanged();
    void durationChanged();
    void minimumTickSpacingChanged();
    void majorTicksLevelChanged();
    void majorTicksChanged();
    void minorTicksChanged();
    void timeZoneChanged();

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace timeline
} // namespace nx::vms::client::core
