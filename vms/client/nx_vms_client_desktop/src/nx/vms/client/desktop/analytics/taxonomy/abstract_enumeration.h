// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::vms::client::desktop::analytics::taxonomy {

class AbstractEnumeration: public QObject
{
    Q_OBJECT
    Q_PROPERTY(std::vector<QString> items READ items CONSTANT)

public:
    AbstractEnumeration(QObject* parent): QObject(parent) {}

    virtual ~AbstractEnumeration() {};

    virtual std::vector<QString> items() const = 0;
};

} // namespace nx::vms::client::desktop::analytics::taxonomy
