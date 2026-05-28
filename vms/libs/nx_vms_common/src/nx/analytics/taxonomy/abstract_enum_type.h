// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>
#include <string>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <nx/vms/api/analytics/descriptors.h>

namespace nx::analytics::taxonomy {

class NX_VMS_COMMON_API AbstractEnumType: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString id READ idAsQString CONSTANT)
    Q_PROPERTY(QString name READ nameAsQString CONSTANT)
    Q_PROPERTY(nx::analytics::taxonomy::AbstractEnumType* baseType READ base CONSTANT)
    Q_PROPERTY(QStringList items READ itemsAsQStringList CONSTANT)
    Q_PROPERTY(QStringList ownItems READ ownItemsAsQStringList CONSTANT)

public:
    AbstractEnumType(QObject* parent = nullptr):
        QObject(parent)
    {
    }

    virtual ~AbstractEnumType() {}

    virtual const std::string& id() const = 0;

    virtual const std::string& name() const = 0;

    virtual AbstractEnumType* base() const = 0;

    virtual std::vector<std::string> ownItems() const = 0;

    virtual std::vector<std::string> items() const = 0;

    virtual nx::vms::api::analytics::EnumTypeDescriptor serialize() const = 0;

private:
    QString idAsQString() const;
    QString nameAsQString() const;
    QStringList ownItemsAsQStringList() const;
    QStringList itemsAsQStringList() const;
};

} // namespace nx::analytics::taxonomy
