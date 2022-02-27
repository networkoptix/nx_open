// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QObject>
#include <QtCore/QString>

#include <nx/vms/api/analytics/descriptors.h>

namespace nx::analytics::taxonomy {

class NX_VMS_COMMON_API AbstractColorType: public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString id READ id CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(nx::analytics::taxonomy::AbstractColorType* baseType READ base CONSTANT)
    Q_PROPERTY(std::vector<QString> items READ items CONSTANT)
    Q_PROPERTY(std::vector<QString> ownItems READ ownItems CONSTANT)
    Q_PROPERTY(std::vector<QString> baseItems READ baseItems CONSTANT)

public:
    struct Item
    {
        QString name;
        QString rgb;
    };

public:
    AbstractColorType(QObject* parent = nullptr):
        QObject(parent)
    {
    }

    virtual ~AbstractColorType() {}

    virtual QString id() const = 0;

    virtual QString name() const = 0;

    virtual AbstractColorType* base() const = 0;

    virtual std::vector<QString> baseItems() const = 0;

    virtual std::vector<QString> ownItems() const = 0;

    virtual std::vector<QString> items() const = 0;

    virtual nx::vms::api::analytics::ColorTypeDescriptor serialize() const = 0;

    Q_INVOKABLE virtual QString color(const QString& item) const = 0;
};

} // namespace nx::analytics::taxonomy
