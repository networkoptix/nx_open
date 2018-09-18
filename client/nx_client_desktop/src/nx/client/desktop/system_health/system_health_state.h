#pragma once

#include <QtCore/QObject>
#include <QtCore/QScopedPointer>

#include <health/system_health.h>
#include <ui/workbench/workbench_context_aware.h>

namespace nx::client::desktop {

class SystemHealthState:
    public QObject,
    public QnWorkbenchContextAware
{
    Q_OBJECT

public:
    SystemHealthState(QObject* parent = nullptr);
    virtual ~SystemHealthState() override;

    using SystemHealthIndex = QnSystemHealth::MessageType;
    bool value(SystemHealthIndex index) const;

    QVariant data(SystemHealthIndex index) const;

signals:
    void changed(SystemHealthIndex index, bool value, QPrivateSignal);

private:
    class Private;
    const QScopedPointer<Private> d;
};

} // namespace nx::client::desktop
