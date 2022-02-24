// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <memory>
#include <chrono>

#include <QtCore/QObject>

class QTimer;

namespace nx::monitoring { class ActivityMonitor; }

namespace nx::vms::client::desktop {

/**
 * Periodically computes and send performance metrics.
 * This class is moved to a separate thread by PerformanceMonitor because otherwise computing
 * some metrics may block main loop for too much time.
 */
class PerformanceWorker: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    static constexpr std::chrono::seconds kUpdateInterval{1};

    PerformanceWorker(QObject* parent = nullptr);
    ~PerformanceWorker();

public slots:
    void start();

signals:
    void valuesChanged(const QVariantMap&);

private slots:
    void gatherValues();

private:
    std::unique_ptr<monitoring::ActivityMonitor> m_monitor;
    std::unique_ptr<QTimer> m_timer;
};

} // namespace nx::vms::client::desktop
