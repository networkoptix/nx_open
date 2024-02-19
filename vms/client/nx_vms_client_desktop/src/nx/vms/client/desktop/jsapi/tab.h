// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QObject>

#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>

#include "globals.h"

class QnWorkbenchLayout;

namespace nx::vms::client::desktop {

class WindowContext;

namespace jsapi {

namespace detail { class TabApiBackend; }

/** Class proxies calls from a JS script to the tab management backend. */
class Tab: public QObject
{
    Q_OBJECT
    using base_type = QObject;

    /**
     * Name of the tab.
     */
    Q_PROPERTY(QString name READ name CONSTANT)

    /**
     * Id of the tab.
     */
    Q_PROPERTY(QString id READ id CONSTANT)

public:
    Tab(WindowContext* context, QnWorkbenchLayout* layout = nullptr, QObject* parent = nullptr);
    virtual ~Tab() override;

    /** State of the tab, including all items and layout information. */
    Q_INVOKABLE QJsonObject state() const;

    /** Description of the item which includes item parameters and corresponding resource. */
    Q_INVOKABLE QJsonObject item(const QUuid& itemId) const;

    /** Add an item with the specified resource id and parameters to the owning layout. */
    Q_INVOKABLE QJsonObject addItem(const QString& resourceId, const QJsonObject& params);

    /** Set specified parameters for the item. */
    Q_INVOKABLE QJsonObject setItemParams(const QUuid& itemId, const QJsonObject& params);

    /** Remove specified item by id. */
    Q_INVOKABLE QJsonObject removeItem(const QUuid& itemId);

    /** Start layout synchronization with the specified item, if available. */
    Q_INVOKABLE QJsonObject syncWith(const QUuid& itemId);

    /** Stop syncing of the corresponding layout. */
    Q_INVOKABLE QJsonObject stopSyncPlay();

    /** Set parameters for the current layout of the tab. */
    Q_INVOKABLE QJsonObject setLayoutProperties(const QJsonObject& properties);

    /** Save layout. */
    Q_INVOKABLE QJsonObject saveLayout();

    QString id() const;
    QString name() const;
    QnWorkbenchLayout* layout() const;

signals:
    void itemAdded(const QJsonObject& item);
    void itemRemoved(const nx::Uuid& itemId);
    void itemChanged(const QJsonObject& item);

private:
    nx::utils::ImplPtr<detail::TabApiBackend> d;
};

} // namespace jsapi
} // namespace nx::vms::client::desktop
