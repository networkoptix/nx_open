// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QModelIndex>
#include <QtCore/QObject>
#include <QtCore/QPersistentModelIndex>

#include <core/resource/resource_fwd.h>

#include "object_data.h"

Q_MOC_INCLUDE("core/resource/resource.h")

namespace nx::vms::client::mobile {

/**
 * Object data of an event search model row, kept up to date with the model.
 */
class EventSearchObjectData: public QObject
{
    Q_OBJECT
    Q_PROPERTY(nx::vms::client::mobile::timeline::ObjectData data READ data NOTIFY dataChanged)
    Q_PROPERTY(QnResource* resource READ rawResource CONSTANT)

public:
    explicit EventSearchObjectData(const QModelIndex& index, QObject* parent = nullptr);

    const timeline::ObjectData& data() const;
    QnResourcePtr resource() const;

    /** Returns whether the model row contains a bookmark or an analytics track. */
    bool isValid() const;

signals:
    void dataChanged();

private:
    void update();
    QnResource* rawResource() const;

private:
    const QPersistentModelIndex m_index;
    const QnResourcePtr m_resource;
    timeline::ObjectData m_data;
    bool m_valid = false;
};

class ObjectDataAdapter: public QObject
{
    Q_OBJECT

public:
    /** Returns a new object with QML ownership, or null if the row has no suitable data. */
    Q_INVOKABLE nx::vms::client::mobile::EventSearchObjectData* create(const QModelIndex& index);

    static void registerQmlType();
};

} // namespace nx::vms::client::mobile
