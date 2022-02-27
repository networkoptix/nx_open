// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QVector>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtGui/QPaintEvent>
#include <QtWidgets/QWidget>

#include <ui/graphics/instruments/instrument.h>

namespace nx::vms::client::desktop {

/**
 * Wraps all measurements for a specific metric.
 */
class DebugMetric
{
public:
    DebugMetric(std::chrono::milliseconds interval, bool profileMode = true);
    virtual ~DebugMetric() = default;

    /** Called by DebugInfoInstrument to get output data. */
    virtual void submitData(QStringList& debugInfoLines, bool toLog) = 0;

    /** Called periodically to update internal data. */
    virtual void updateData() = 0;

    void restart();
    void invalidate();
    bool needProfileMode() const;

    /** Checks if internal data was updated. */
    bool check();

protected:
    QElapsedTimer timer;
    std::chrono::milliseconds updateInterval;
    bool profileMode = true;
};

class DebugInfoInstrument: public Instrument
{
    Q_OBJECT

public:
    explicit DebugInfoInstrument(QObject* parent = nullptr);
    virtual ~DebugInfoInstrument() override;

    std::vector<qint64> getFrameTimePoints();
    void addDebugMetric(std::unique_ptr<DebugMetric> metric);

signals:
    void debugInfoChanged(const QString& richText);

protected:
    virtual void enabledNotify() override;
    virtual void aboutToBeDisabledNotify() override;

    virtual bool paintEvent(QWidget* viewport, QPaintEvent* event) override;

private:
    struct Private;
    QScopedPointer<Private> d;
};

} // nx::vms::client::desktop
