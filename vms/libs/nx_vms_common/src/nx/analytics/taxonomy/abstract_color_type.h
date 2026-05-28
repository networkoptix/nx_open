// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>
#include <string>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <nx/vms/api/analytics/descriptors.h>

namespace nx::analytics::taxonomy {

class NX_VMS_COMMON_API AbstractColorType: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString id READ idAsQString CONSTANT)
    Q_PROPERTY(QString name READ nameAsQString CONSTANT)
    Q_PROPERTY(nx::analytics::taxonomy::AbstractColorType* baseType READ base CONSTANT)
    Q_PROPERTY(QStringList items READ itemsAsQStringList CONSTANT)
    Q_PROPERTY(QStringList ownItems READ ownItemsAsQStringList CONSTANT)
    Q_PROPERTY(QStringList baseItems READ baseItemsAsQStringList CONSTANT)

public:
    struct Item
    {
        std::string name;
        std::string rgb;
    };

public:
    AbstractColorType(QObject* parent = nullptr):
        QObject(parent)
    {
    }

    virtual ~AbstractColorType() {}

    virtual const std::string& id() const = 0;

    virtual const std::string& name() const = 0;

    virtual AbstractColorType* base() const = 0;

    virtual std::vector<std::string> baseItems() const = 0;

    virtual std::vector<std::string> ownItems() const = 0;

    virtual std::vector<std::string> items() const = 0;

    virtual nx::vms::api::analytics::ColorTypeDescriptor serialize() const = 0;

    Q_INVOKABLE virtual std::string color(const std::string& item) const = 0;

private:
    QString idAsQString() const;
    QString nameAsQString() const;
    QStringList itemsAsQStringList() const;
    QStringList ownItemsAsQStringList() const;
    QStringList baseItemsAsQStringList() const;
};

} // namespace nx::analytics::taxonomy
