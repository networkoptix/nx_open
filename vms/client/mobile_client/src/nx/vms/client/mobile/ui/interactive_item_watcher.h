// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <unordered_map>

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtQuick/QQuickItem>

#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/context_from_qml_handler.h>
#include <nx/vms/client/mobile/window_context_aware.h>

namespace nx::vms::client::mobile {

class InteractiveItemAttached;
class InteractiveItemRegistry;

class InteractiveItemWatcher:
    public QObject,
    public WindowContextAware,
    public core::ContextFromQmlHandler
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)

public:
    InteractiveItemWatcher(QObject* parent = nullptr);

    QString name() const { return m_name; }
    void setName(const QString& name);

    static void registerQmlType();

signals:
    void nameChanged();
    void available(QQuickItem* item);
    void interacted(QQuickItem* item);

protected:
    virtual void onContextReady() override;

private:
    void add(InteractiveItemAttached* item);
    void remove(InteractiveItemAttached* item);
    void reset();

private:
    bool m_ready = false;
    QPointer<InteractiveItemRegistry> m_registry;
    QString m_name;
    std::unordered_map<InteractiveItemAttached*, nx::utils::ScopedConnections> m_connections;
};

} // namespace nx::vms::client::mobile
