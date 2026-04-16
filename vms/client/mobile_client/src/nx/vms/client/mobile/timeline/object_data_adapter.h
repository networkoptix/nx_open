// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QModelIndex>
#include <QtCore/QObject>

#include "abstract_object_data.h"

namespace nx::vms::client::mobile {

class ObjectDataAdapter: public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE timeline::AbstractObjectData* create(const QModelIndex& index);

    static void registerQmlType();
};

} // namespace nx::vms::client::mobile
