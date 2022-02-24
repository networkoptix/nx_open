// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QObject>
#include <QtCore/QString>

#include <nx/vms/api/analytics/descriptors.h>

namespace nx::analytics::taxonomy {

class NX_VMS_COMMON_API AbstractEnumType: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(nx::analytics::taxonomy::AbstractEnumType* baseType READ base CONSTANT)
    Q_PROPERTY(std::vector<QString> items READ items CONSTANT)
    Q_PROPERTY(std::vector<QString> ownItems READ ownItems CONSTANT)

public:
    AbstractEnumType(QObject* parent = nullptr):
        QObject(parent)
    {
    }

    virtual ~AbstractEnumType() {}

    virtual QString id() const = 0;

    virtual QString name() const = 0;

    virtual AbstractEnumType* base() const = 0;

    virtual std::vector<QString> ownItems() const = 0;

    virtual std::vector<QString> items() const = 0;

    virtual nx::vms::api::analytics::EnumTypeDescriptor serialize() const = 0;
};

} // namespace nx::analytics::taxonomy
