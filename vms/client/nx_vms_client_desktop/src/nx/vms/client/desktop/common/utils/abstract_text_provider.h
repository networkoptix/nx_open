// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::vms::client::desktop {

class AbstractTextProvider: public QObject
{
    Q_OBJECT

public:
    using QObject::QObject; //< Forward constructors.

    virtual QString text() const = 0;

signals:
    void textChanged(const QString& value);
};

} // namespace nx::vms::client::desktop
