// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::vms::client::desktop::analytics::taxonomy {

class AbstractColorSet: public QObject
{
    Q_OBJECT
    Q_PROPERTY(std::vector<QString> items READ items CONSTANT)

public:
    AbstractColorSet(QObject* parent): QObject(parent) {}

    virtual ~AbstractColorSet() {};

    virtual std::vector<QString> items() const = 0;

    Q_INVOKABLE virtual QString color(const QString& item) const = 0;
};

} // namespace nx::vms::client::desktop::analytics::taxonomy
