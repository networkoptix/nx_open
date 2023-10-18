// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QVariantMap>

#include <nx/monitoring/activity_monitor.h>
#include <nx/utils/impl_ptr.h>

namespace nx::vms::client::desktop {

/**
 * Provides rich text with performance metrics, including FPS.
 * Each visual element with its own FPS should create a new instance of this class and register
 * its frame rendering by calling registerFrame() slot.
 * Values are updated with the interval of PerfomanceMonitor class.
 * Visibility has the same value for all instances (show/hide application-wise) and is controlled
 * via globlal PerfomanceMonitor instance.
 */
class PerformanceInfo: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    Q_PROPERTY(QString text READ text NOTIFY textChanged)
    Q_PROPERTY(bool visible READ isVisible NOTIFY visibleChanged)

public:
    PerformanceInfo(QObject* parent = nullptr);
    virtual ~PerformanceInfo();

    QString text() const;
    bool isVisible() const;

public slots:
    void registerFrame();

signals:
    void textChanged(const QString&);
    void visibleChanged();

private slots:
    void setPerformanceValues(const QVariantMap& values);

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
