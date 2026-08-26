// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtQml/QQmlProperty>
#include <QtQmlIntegration/QtQmlIntegration>
#include <QtQuick/QQuickItem>

#include <nx/utils/pending_operation.h>
#include <nx/utils/scoped_connections.h>

namespace nx::vms::client::mobile {

class InteractiveItemAttached;

/**
 * Registry for QML items with the InteractiveItem attached type.
 */
class InteractiveItemRegistry: public QObject
{
    Q_OBJECT
    QML_ATTACHED(InteractiveItemAttached)

public:
    InteractiveItemRegistry(QObject* parent = nullptr);
    virtual ~InteractiveItemRegistry() override;

    void add(const QString& name, InteractiveItemAttached* item);
    void remove(const QString& name, InteractiveItemAttached* item);

    QList<InteractiveItemAttached*> attachedItems(const QString& name) const;
    Q_INVOKABLE QList<QQuickItem*> items(const QString& name) const;

    static InteractiveItemAttached* qmlAttachedProperties(QObject* object);
    static void registerQmlType();

signals:
    void itemAdded(const QString& name, InteractiveItemAttached* item);
    void itemRemoved(const QString& name, InteractiveItemAttached* item);

private:
    QMultiHash<QString, InteractiveItemAttached*> m_items;
};

/**
 * The InteractiveItem attached type allows registering Item in InteractiveItemRegistry of the
 * current window context and monitoring its availability and interactions.
 */
class InteractiveItemAttached: public QObject
{
    Q_OBJECT

    /**
     * Name of the interactive item used in the registry.
     */
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)

    /**
     * Source of interaction signals. By default, the item itself is used as the source of one of
     * the following signals: `clicked`, `tapped` (if defined).
     */
    Q_PROPERTY(QObject* interactionSource READ interactionSource WRITE setInteractionSource NOTIFY
            interactionSourceChanged)

    /**
     * Whether the item is available, which is determined by its properties: `visible`, `enabled`
     * and `interactive` (if defined).
     */
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)

public:
    InteractiveItemAttached(InteractiveItemRegistry* registry, QQuickItem* parent);
    virtual ~InteractiveItemAttached() override;

    QString name() const { return m_name; }
    void setName(const QString& name);

    void setInteractionSource(QObject* source);
    QObject* interactionSource() const { return m_interactionSource; }

    bool available() const { return m_available; }

    QQuickItem* item() const { return m_item; }

signals:
    void nameChanged();
    void interactionSourceChanged();

    void availableChanged();
    void interacted();

private slots:
    void requestUpdate();
    void updateAvailability();
    void updateInteractionConnection();

private:
    QQuickItem* const m_item;
    const QPointer<InteractiveItemRegistry> m_registry;
    QString m_name;
    QPointer<QObject> m_interactionSource;
    QQmlProperty m_interactive;
    bool m_available = false;
    nx::utils::PendingOperation m_update;
    nx::utils::ScopedConnection m_interactionConnection;
};

} // namespace nx::vms::client::mobile
