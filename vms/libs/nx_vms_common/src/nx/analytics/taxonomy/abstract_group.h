// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <nx/vms/api/analytics/descriptors.h>

namespace nx::analytics::taxonomy {

class NX_VMS_COMMON_API AbstractGroup: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)

public:
    AbstractGroup(QObject* parent):
        QObject(parent)
    {
    }

    virtual ~AbstractGroup() {}

    virtual QString id() const = 0;

    virtual QString name() const = 0;

    virtual nx::vms::api::analytics::GroupDescriptor serialize() const = 0;
};

} // namespace nx::analytics::taxonomy
