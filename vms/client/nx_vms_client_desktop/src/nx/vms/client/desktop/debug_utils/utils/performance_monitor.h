// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <QtCore/QObject>
#include <QtCore/QVariantMap>

#include <nx/utils/impl_ptr.h>

/**
 * Application-global class that periodically sends a signal with performance metrics.
 * It does not produce the metrics if they are not visible or not recorded to trace file.
 */
namespace nx::vms::client::desktop {

class PerformanceMonitor: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    PerformanceMonitor(QObject* parent = nullptr);
    virtual ~PerformanceMonitor();

    QVariantMap values() const;
    bool isVisible() const;
    void setDebugInfoVisible(bool visible);
    bool isDebugInfoVisible() const;

    static const QString kCpu;
    static const QString kGpu;
    static const QString kMemory;
    static const QString kThreads;

public slots:
    void setVisible(bool visible);
    void setEnabled(bool enabled);

signals:
    void valuesChanged(const QVariantMap&);
    void visibleChanged(bool visible);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;
};

} // namespace nx::vms::client::desktop
