// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

namespace nx::vms::client::mobile::test {

class TestUtils: public QObject
{
    Q_OBJECT

public:
    /** Finds an object by QML id in the type of `root`, its base types and nested components. */
    Q_INVOKABLE QObject* findById(QObject* root, const QString& id) const;

    static void registerQmlType();
};

} // namespace nx::vms::client::mobile::test
